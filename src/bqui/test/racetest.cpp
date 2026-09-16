// Real-GL (threaded) reproduction of the withAnimation crash, fully unattended.
//
// Runs App::headless(true) -- the REAL WGL/GLX backend, whose main render queue
// executes on a GlDispatchedContext Dispatcher worker thread -- and drives rapid
// withAnimation-triggering collection mutations (mimicking the adder's T/S/x
// buttons) while animation frames render. The loop thread's withAnimation ->
// AnimationGuard -> WindowBridge::makeTransaction is meant to overlap the
// Dispatcher thread's command execution and fault/hang.
//
// SAFETY: this test can abort/crash by design. On Windows every failure dialog
// (CRT assert box, abort() box, Windows Error Reporting GP-fault box) is
// suppressed up front, and abort/terminate/unhandled-SEH are trapped to dump
// EVERY thread's call stack to stderr and then _exit -- so it never blocks the
// machine and the racing stacks land in the log.
//
// NOT a registered test: it needs a real GPU the CI runners lack. Run by hand
// from the repo root so 'data/fonts/OpenSans-Regular.ttf' resolves.

#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/withanimation.h>

#include <bqui/widget/label.h>
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

#include <avg/curve/curves.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <crtdbg.h>
#endif

using namespace bqui;

// ---------------------------------------------------------------------------
// Unattended crash capture (Windows): dump all threads' stacks, then _exit.
// ---------------------------------------------------------------------------
#ifdef _WIN32
namespace
{
    // Deadlock-free: only ever walks the CURRENT thread via CaptureStackBackTrace
    // (no SuspendThread of other threads, so it cannot wedge on a lock the GL
    // driver holds). For the concurrent thread's stack, attach cdb externally.
    void dumpCurrentThread(char const* reason)
    {
        std::fflush(stdout);
        std::fprintf(stderr, "\n================ CRASH: %s ================\n",
                reason);

        HANDLE proc = GetCurrentProcess();
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS
                | SYMOPT_UNDNAME);
        SymInitialize(proc, nullptr, TRUE);

        void* frames[62];
        USHORT n = CaptureStackBackTrace(0, 62, frames, nullptr);

        char symbolBuffer[sizeof(SYMBOL_INFO) + 512];
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);

        std::fprintf(stderr, "--- faulting thread %lu ---\n",
                (unsigned long)GetCurrentThreadId());

        for (USHORT i = 0; i < n; ++i)
        {
            std::memset(symbol, 0, sizeof(symbolBuffer));
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 500;

            DWORD64 disp = 0;
            char const* name = "??";
            if (SymFromAddr(proc, (DWORD64)frames[i], &disp, symbol))
                name = symbol->Name;

            IMAGEHLP_LINE64 line;
            std::memset(&line, 0, sizeof(line));
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisp = 0;
            if (SymGetLineFromAddr64(proc, (DWORD64)frames[i], &lineDisp, &line))
                std::fprintf(stderr, "  %2u  %s  (%s:%lu)\n", i, name,
                        line.FileName, (unsigned long)line.LineNumber);
            else
                std::fprintf(stderr, "  %2u  %s+0x%llx\n", i, name,
                        (unsigned long long)disp);
        }

        std::fprintf(stderr, "================ END CRASH DUMP ================\n");
        std::fflush(stderr);
    }

    LONG WINAPI onUnhandledSeh(EXCEPTION_POINTERS* ep)
    {
        char reason[64];
        std::snprintf(reason, sizeof(reason), "SEH exception 0x%08lx",
                (unsigned long)ep->ExceptionRecord->ExceptionCode);
        dumpCurrentThread(reason);
        _exit(2);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void onAbort(int)
    {
        dumpCurrentThread("abort() / SIGABRT");
        _exit(134);
    }

    void onTerminate()
    {
        char const* what = "std::terminate (unknown)";
        std::string msg;
        if (auto e = std::current_exception())
        {
            try { std::rethrow_exception(e); }
            catch (std::exception const& ex)
            {
                msg = std::string("std::terminate: ") + ex.what();
                what = msg.c_str();
            }
            catch (...) { what = "std::terminate: non-std exception"; }
        }
        dumpCurrentThread(what);
        _exit(3);
    }

    void installUnattendedCrashHandlers()
    {
        // No modal dialogs, ever.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX
                | SEM_NOOPENFILEERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);

        SetUnhandledExceptionFilter(onUnhandledSeh);
        std::signal(SIGABRT, onAbort);
        std::set_terminate(onTerminate);
    }
} // namespace
#else
namespace { void installUnattendedCrashHandlers() {} }
#endif

namespace
{
    widget::AnyWidget row(bq::signal::AnySignal<std::string> value)
    {
        return widget::hbox({ widget::label(std::move(value)) })
            | modifier::transition(modifier::transitionLeft())
            | modifier::clip();
    }
} // namespace

int main(int argc, char** argv)
{
    installUnattendedCrashHandlers();

    // argv[1] = max frames, argv[2] = mutation spacing (mutate every Nth frame;
    // >1 lets animations settle between mutations like real clicks),
    // argv[3] = wall-clock budget seconds (self-terminates so it never hangs).
    int churnFrames = argc > 1 ? std::atoi(argv[1]) : 400;
    int spacing = argc > 2 ? std::atoi(argv[2]) : 1;
    if (spacing < 1) spacing = 1;
    double budgetSec = argc > 3 ? std::atof(argv[3]) : 30.0;

    Collection<std::string> items;
    {
        auto range = items.rangeLock();
        range.pushBack("test 1");
        range.pushBack("test 2");
        range.pushBack("test 3");
        range.pushBack("test 4");
    }

    auto widgets = dataBind<std::string>(
            dataSourceFromCollection(items),
            [](bq::signal::AnySignal<std::string> value, size_t) -> widget::AnyWidget
            {
                return row(std::move(value));
            });

    App app;
    app.headless(true);

    Window w = window(bq::signal::constant<std::string>("race"));
    app.addWindow(w, widget::vbox(std::move(widgets)));

    int frames = 0;
    int mutations = 0;
    auto start = std::chrono::steady_clock::now();
    auto lastTick = start;
    double firstFrameMs = 0.0;
    double lastFrameMs = 0.0;
    bool budgetHit = false;
    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                auto now = std::chrono::steady_clock::now();
                double frameMs = std::chrono::duration<double, std::milli>(
                        now - lastTick).count();
                lastTick = now;
                if (frames == 1) firstFrameMs = frameMs;
                lastFrameMs = frameMs;
                ++frames;

                double elapsed = std::chrono::duration<double>(now - start)
                        .count();
                if (elapsed > budgetSec)
                {
                    budgetHit = true;
                    return false;
                }

                if (i >= churnFrames)
                    return false;

                // A withAnimation-wrapped mutation, the shape of the adder's
                // T/S/x handlers, fired every `spacing` frames. Cycle swap /
                // move-to-front / erase+add so the tree keeps reordering.
                if (i % spacing == 0)
                {
                    auto a = withAnimation(0.3f, avg::curve::linear);
                    auto range = items.rangeLock();

                    if (range.size() >= 2)
                    {
                        switch (mutations % 3)
                        {
                        case 0:
                            range.swap(range.begin(), range.begin() + 1);
                            break;
                        case 1:
                            range.move(range.begin() + 1, range.begin());
                            break;
                        case 2:
                        {
                            auto id = range.begin().getId();
                            range.eraseWithId(id);
                            range.pushFront("test " + std::to_string(i));
                            break;
                        }
                        }
                        ++mutations;
                    }
                }

                step.handle.set(i + 1);
                return true;
            });

    int rc = app.run(running);

    std::cout << "race run: frames=" << frames << " mutations=" << mutations
        << " rc=" << rc
        << (budgetHit ? " [BUDGET HIT -> likely growth/hang]" : "")
        << " firstFrameMs=" << firstFrameMs
        << " lastFrameMs=" << lastFrameMs << std::endl;
    return rc;
}
