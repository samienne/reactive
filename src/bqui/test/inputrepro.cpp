// FAITHFUL input-driven repro of the adder T/S/x crash/hang.
//
// Uses the SINGLETON app() (so the free withAnimation() actually transacts these
// windows -- the fix for last round's local-App artifact) and drives REAL
// pointer clicks through the genuine hit-test -> onClick path (App::debugInjectClick
// -> WindowBridge::injectPointerButton -> aseWindow button callback -> input-area
// hit-test -> the row's onClick handler, which runs withAnimation on the loop
// thread). NOT calling withAnimation from the frame callback.
//
// Rows are a dataBind/Collection reconstruction of the adder row: T (move-to-
// front), S (swap-two, shared swapState), x (erase) -- each wrapped in
// withAnimation, exactly like src/testapp1/adder.cpp.
//
// Backend: argv[5]=1 -> real WGL offscreen (headless); default dummy (the
// hang/leak is algorithmic). Args: [1]=maxSteps [2]=framesPerAction
// [3]=budgetSec [4]=? [5]=realGl.
//
// SAFETY: suppresses every Windows failure dialog and dumps the faulting
// thread's stack (deadlock-free CaptureStackBackTrace) before _exit, so it never
// blocks the machine; attach cdb externally for the GL Dispatcher thread's stack.

#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/withanimation.h>

#include <bqui/widget/label.h>
#include <bqui/widget/button.h>
#include <bqui/widget/filler.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/vbox.h>

#include <bqui/modifier/transition.h>
#include <bqui/modifier/clip.h>

#include <bqui/collection.h>
#include <bqui/datasourcefromcollection.h>
#include <bqui/datasource.h>
#include <bqui/databind.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <ase/dummyplatform.h>
#include <btl/runloop.h>

#include <avg/curve/curves.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
#endif

using namespace bqui;

// ---- unattended, deadlock-free crash capture (current thread only) ----------
#ifdef _WIN32
namespace
{
    void dumpCurrentThread(char const* reason)
    {
        std::fflush(stdout);
        std::fprintf(stderr, "\n======== CRASH: %s ========\n", reason);
        HANDLE proc = GetCurrentProcess();
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
        SymInitialize(proc, nullptr, TRUE);
        void* frames[62];
        USHORT n = CaptureStackBackTrace(0, 62, frames, nullptr);
        char buf[sizeof(SYMBOL_INFO) + 512];
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        std::fprintf(stderr, "--- faulting thread %lu ---\n",
                (unsigned long)GetCurrentThreadId());
        for (USHORT i = 0; i < n; ++i)
        {
            std::memset(sym, 0, sizeof(buf));
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = 500;
            DWORD64 disp = 0;
            char const* name = "??";
            if (SymFromAddr(proc, (DWORD64)frames[i], &disp, sym))
                name = sym->Name;
            IMAGEHLP_LINE64 line;
            std::memset(&line, 0, sizeof(line));
            line.SizeOfStruct = sizeof(line);
            DWORD ld = 0;
            if (SymGetLineFromAddr64(proc, (DWORD64)frames[i], &ld, &line))
                std::fprintf(stderr, "  %2u  %s  (%s:%lu)\n", i, name,
                        line.FileName, (unsigned long)line.LineNumber);
            else
                std::fprintf(stderr, "  %2u  %s+0x%llx\n", i, name,
                        (unsigned long long)disp);
        }
        std::fprintf(stderr, "======== END DUMP ========\n");
        std::fflush(stderr);
    }

    LONG WINAPI onSeh(EXCEPTION_POINTERS* ep)
    {
        char r[64];
        std::snprintf(r, sizeof(r), "SEH 0x%08lx",
                (unsigned long)ep->ExceptionRecord->ExceptionCode);
        dumpCurrentThread(r);
        _exit(2);
        return EXCEPTION_EXECUTE_HANDLER;
    }
    void onAbort(int) { dumpCurrentThread("abort/SIGABRT"); _exit(134); }
    void onTerminate()
    {
        std::string m = "terminate";
        if (auto e = std::current_exception())
        {
            try { std::rethrow_exception(e); }
            catch (std::exception const& ex) { m = std::string("terminate: ") + ex.what(); }
            catch (...) { m = "terminate: non-std"; }
        }
        dumpCurrentThread(m.c_str());
        _exit(3);
    }
    void installHandlers()
    {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX
                | SEM_NOOPENFILEERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        SetUnhandledExceptionFilter(onSeh);
        std::signal(SIGABRT, onAbort);
        std::set_terminate(onTerminate);
    }
} // namespace
#else
namespace { void installHandlers() {} }
#endif

namespace
{
    // Counts every real handler invocation, proving injected clicks land on the
    // genuine onClick path.
    std::shared_ptr<std::atomic<long>> g_handlerCalls;

    // RACE_BENIGN=1: handlers only count (no withAnimation, no list mutation),
    // so no widget is ever rebuilt/freed. If clicks still crash then, the fault
    // is in the click/hit-test path itself, not a mutation-driven dangling
    // handler.
    bool g_benign = false;

    // Raw-lambda button handlers, exactly as src/testapp1/adder.cpp builds them
    // (no explicit std::function wrapping), to stay faithful under #150's
    // AnySignal<void()>.
    widget::AnyWidget adderRow(bq::signal::AnySignal<std::string> value,
            size_t id, Collection<std::string> items,
            std::shared_ptr<size_t> swapState)
    {
        auto calls = g_handlerCalls;

        return widget::hbox({
                widget::button("T", bq::signal::constant([items, id, calls]() mutable
                    {
                        ++*calls;
                        if (g_benign) return;
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        auto range = items.rangeLock();
                        auto i = range.findId(id);
                        range.move(i, range.begin());
                    })),
                widget::button("S", bq::signal::constant(
                    [items, id, swapState, calls]() mutable
                    {
                        ++*calls;
                        if (g_benign) return;
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        if (*swapState == 0)
                        {
                            *swapState = id;
                        }
                        else
                        {
                            auto range = items.rangeLock();
                            auto i = range.findId(id);
                            auto j = range.findId(*swapState);
                            range.swap(i, j);
                            *swapState = 0;
                        }
                    })),
                widget::label(std::move(value)),
                widget::hfiller(),
                widget::button("x", bq::signal::constant([items, id, calls]() mutable
                    {
                        ++*calls;
                        if (g_benign) return;
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        items.rangeLock().eraseWithId(id);
                    }))
                })
            | modifier::transition(modifier::transitionLeft())
            | modifier::clip();
    }
} // namespace

int main(int argc, char** argv)
{
    // Skip our in-process handlers when we want ASan (or an external debugger)
    // to catch the fault and report allocation/free provenance.
    if (!std::getenv("RACE_NO_HANDLERS"))
        installHandlers();

    g_benign = std::getenv("RACE_BENIGN") != nullptr;

    int maxSteps = argc > 1 ? std::atoi(argv[1]) : 2000;
    int framesPerAction = argc > 2 ? std::atoi(argv[2]) : 6;
    if (framesPerAction < 1) framesPerAction = 1;
    double budgetSec = argc > 3 ? std::atof(argv[3]) : 60.0;
    bool realGl = argc > 5 && std::atoi(argv[5]) != 0;

    g_handlerCalls = std::make_shared<std::atomic<long>>(0);

    Collection<std::string> items;
    auto swapState = std::make_shared<size_t>(0);
    int addCounter = 0;
    {
        auto range = items.rangeLock();
        for (; addCounter < 5; ++addCounter)
            range.pushBack("row " + std::to_string(addCounter));
    }

    auto widgets = dataBind<std::string>(
            dataSourceFromCollection(items),
            [items, swapState](bq::signal::AnySignal<std::string> value, size_t id)
                    mutable -> widget::AnyWidget
            {
                return adderRow(std::move(value), id, items, swapState);
            });

    btl::RunLoop loop;
    App app = bqui::app();          // the app withAnimation() transacts
    if (realGl)
        app.headless(true);
    else
        app.platform(ase::makeDummyPlatform(loop));

    Window w = window(bq::signal::constant<std::string>("inputrepro"));
    app.addWindow(w, widget::vbox(std::move(widgets)));

    int frames = 0;
    int actions = 0;
    int sPhase = 0;                 // 0 = first S click pending, 1 = second
    long lastHandlerCalls = 0;
    int missedClicks = 0;
    auto start = std::chrono::steady_clock::now();
    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                ++frames;
                if (std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - start).count()
                        > budgetSec)
                    return false;
                if (actions >= maxSteps)
                    return false;

                // Let the window mount and settle for a few frames first.
                if (i < 3 || app.debugWindowCount() == 0)
                {
                    step.handle.set(i + 1);
                    return true;
                }

                if (i % framesPerAction == 0)
                {
                    // Keep the list populated (adds are not under test; done via
                    // the collection, wrapped in withAnimation for realism).
                    size_t rows = app.debugCountButtons(0, "x");
                    if (rows < 3)
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        auto range = items.rangeLock();
                        range.pushFront("row " + std::to_string(addCounter++));
                    }
                    else if (std::getenv("RACE_DIRECT"))
                    {
                        // Same erase, but via the frame callback (like racetest),
                        // NOT a click -- to test the deep-row propagation on the
                        // exact same widget without the input path.
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        auto range = items.rangeLock();
                        range.eraseWithId(range.begin().getId());
                        range.pushFront("row " + std::to_string(addCounter++));
                        ++actions;
                    }
                    else
                    {
                        // Cycle real clicks: x (erase row0), T (move last->front),
                        // S (row0 then row1 -> swap).
                        float x = 0.f, y = 0.f;
                        bool ok = false;
                        int which = actions % 4;
                        if (which == 0)
                            ok = app.debugFindButton(0, "x", 0, x, y);
                        else if (which == 1)
                            ok = app.debugFindButton(0, "T", rows - 1, x, y);
                        else // 2,3 -> S on row0 then row1
                            ok = app.debugFindButton(0, "S", sPhase, x, y);

                        if (ok)
                        {
                            long before = g_handlerCalls->load();
                            app.debugInjectClick(0, x, y);
                            if (g_handlerCalls->load() == before)
                                ++missedClicks;
                            if (which >= 2)
                                sPhase = sPhase == 0 ? 1 : 0;
                        }
                        ++actions;
                    }
                }

                step.handle.set(i + 1);
                return true;
            });

    int rc = app.run(running);

    std::fprintf(stderr,
        "inputrepro: frames=%d actions=%d handlerCalls=%ld missedClicks=%d "
        "rc=%d realGl=%d\n",
        frames, actions, g_handlerCalls->load(), missedClicks, rc, realGl ? 1 : 0);
    (void)lastHandlerCalls;
    return rc;
}
