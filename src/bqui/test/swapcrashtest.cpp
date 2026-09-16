// Repro harness for the reported "S then S" swap crash in testapp1's adder.
//
// The adder's "S" button remembers one row's id on the first click and, on the
// second click on a DIFFERENT row, performs
//     range.swap(range.findId(idB), range.findId(idA))
// under an active withAnimation, with each row wrapped in a transition+clip.
//
// These tests reconstruct that exact sequence two ways:
//   1. DataPath  - Collection + dataBind driven in a SignalContext, no app,
//                  no animation. Confirms whether the pure data/stream path
//                  crashes on a valid two-row swap.
//   2. RenderPath - the singleton app() running the dummy backend, with an
//                   adder-like widget mounted, performing the withAnimation
//                   swap mid-run so the transition actually animates.
//   3. StaleId   - the unchecked-end() hazard: swap where one findId misses.

#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/withanimation.h>

#include <bqui/widget/label.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/button.h>

#include <bqui/modifier/transition.h>
#include <bqui/modifier/clip.h>

#include <bqui/collection.h>
#include <bqui/datasourcefromcollection.h>
#include <bqui/datasource.h>
#include <bqui/databind.h>

#include <ase/dummyplatform.h>

#include <btl/runloop.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>
#include <bq/signal/signalcontext.h>

#include <avg/curve/curves.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <memory>

using namespace bqui;

namespace
{
    constexpr int maxFrames = 200;

    std::vector<size_t> idsOf(Collection<std::string>& items)
    {
        std::vector<size_t> ids;
        auto range = items.rangeLock();
        for (auto i = range.begin(); i != range.end(); ++i)
            ids.push_back(i.getId());
        return ids;
    }

    Collection<std::string> makeItems()
    {
        Collection<std::string> items;
        auto range = items.rangeLock();
        range.pushBack("test 1");
        range.pushBack("test 2");
        range.pushBack("test 3");
        range.pushBack("test 4");
        return items;
    }
} // namespace

// -------- 1. Pure data path (no app, no animation) --------------------------
TEST(SwapCrash, dataPathValidTwoRowSwap)
{
    Collection<std::string> items = makeItems();

    auto widgets = dataBind<std::string>(
            dataSourceFromCollection(items),
            [](bq::signal::AnySignal<std::string> value, size_t) -> widget::AnyWidget
            {
                return widget::label(std::move(value));
            });

    auto c = bq::signal::makeSignalContext(widgets);
    auto initial = c.evaluate<0>().get<0>();
    ASSERT_EQ(4u, initial.size());

    auto ids = idsOf(items);
    size_t idA = ids[0];
    size_t idB = ids[2];

    {
        auto range = items.rangeLock();
        auto i = range.findId(idB);
        auto j = range.findId(idA);
        range.swap(i, j);
    }

    auto r = c.update(bq::signal::FrameInfo(1, {}));
    auto after = c.evaluate<0>().get<0>();

    EXPECT_TRUE(r.didChange);
    ASSERT_EQ(4u, after.size());
    // ids reordered: idB now first, idA now third.
    EXPECT_EQ(idB, after[0].first);
    EXPECT_EQ(idA, after[2].first);
}

// -------- 3. Unchecked end() hazard (stale id) — THE CRASH ------------------
// Faithful to a real user action sequence in the adder:
//   1. click "S" on row A  -> swapState = idA
//   2. click "x" on row A  -> row A erased, idA now dangling/absent
//   3. click "S" on row B  -> else-branch runs
//          i = findId(idB)          (valid)
//          j = findId(idA)          (STALE -> end())
//          range.swap(i, j);        (no end() check -> deref end())
// In a Debug build this trips assert(a != end() && b != end()) at
// collection.h:371; in Release the assert is gone and swap() dereferences the
// end() iterator (a.getId()/std::swap over data.end()) -> heap corruption.
void staleIdSwapSequence()
{
    Collection<std::string> items = makeItems();

    auto widgets = dataBind<std::string>(
            dataSourceFromCollection(items),
            [](bq::signal::AnySignal<std::string> value, size_t) -> widget::AnyWidget
            {
                return widget::label(std::move(value));
            });

    auto c = bq::signal::makeSignalContext(widgets);
    c.evaluate<0>().get<0>();

    auto ids = idsOf(items);
    size_t idA = ids[0];   // remembered by the first "S"
    size_t idB = ids[2];   // the second "S" row

    // Step 2: erase row A (the "x" button).
    items.rangeLock().eraseWithId(idA);
    c.update(bq::signal::FrameInfo(1, {}));

    // Step 3: second "S" on row B, swapping against the now-stale idA, exactly
    // as adder.cpp does with no end() check.
    {
        auto range = items.rangeLock();
        auto i = range.findId(idB);
        auto j = range.findId(idA);   // end() -- idA was erased
        range.swap(i, j);             // <-- crash: deref of end() iterator
    }

    c.update(bq::signal::FrameInfo(2, {}));
}

TEST(SwapCrash, staleIdAfterEraseCrashesUncheckedSwap)
{
    // Collection::Range::swap dereferences the end() iterator that findId
    // returned for the stale id: abort() via the precondition assert with
    // asserts on (this project's Debug and Release), heap corruption with
    // asserts off. Proven here as a death.
    EXPECT_DEATH(staleIdSwapSequence(), "");
}

// The proposed app-level fix: guard findId != end() before swapping. With the
// guard, the same stale-id sequence is a no-op instead of a crash.
TEST(SwapCrash, guardedSwapSurvivesStaleId)
{
    Collection<std::string> items = makeItems();
    auto ids = idsOf(items);
    size_t idA = ids[0];
    size_t idB = ids[2];

    items.rangeLock().eraseWithId(idA);

    {
        auto range = items.rangeLock();
        auto i = range.findId(idB);
        auto j = range.findId(idA);
        if (i != range.end() && j != range.end())
            range.swap(i, j);
    }

    EXPECT_EQ(3u, items.rangeLock().size());
    SUCCEED();
}

// -------- 4. Faithful captured-delegate-id vs live getId --------------------
// The real button closures capture the `id` dataBind hands the delegate at
// BUILD time and later call range.findId(capturedId). This checks whether that
// captured id still matches the row's live getId() with NO list mutation.
TEST(SwapCrash, capturedDelegateIdMatchesLiveGetId)
{
    Collection<std::string> items = makeItems();

    auto capturedIds = std::make_shared<std::vector<size_t>>();

    auto widgets = dataBind<std::string>(
            dataSourceFromCollection(items),
            [capturedIds](bq::signal::AnySignal<std::string> value, size_t id)
                    -> widget::AnyWidget
            {
                capturedIds->push_back(id);   // exactly what the closures capture
                return widget::label(std::move(value));
            });

    // Evaluating the widget signal runs dataBind's evaluateOnInit, which calls
    // the delegate for each initial row.
    auto c = bq::signal::makeSignalContext(widgets);
    auto widgetList = c.evaluate<0>().get<0>();

    ASSERT_EQ(4u, widgetList.size());
    ASSERT_EQ(4u, capturedIds->size());

    auto liveIds = idsOf(items);

    // (a) Are the ids in the widget list (o.id) equal to live getIds?
    for (size_t k = 0; k < widgetList.size(); ++k)
        EXPECT_EQ(liveIds[k], widgetList[k].first)
            << "widgetList id mismatch at " << k;

    // (b) THE REAL QUESTION: does findId(capturedId) find each present row?
    auto range = items.rangeLock();
    for (size_t k = 0; k < capturedIds->size(); ++k)
    {
        auto it = range.findId((*capturedIds)[k]);
        EXPECT_NE(range.end(), it)
            << "captured id " << (*capturedIds)[k]
            << " (row " << k << ") NOT FOUND though row is present";
    }

    // Report both id sets regardless, for diagnosis.
    for (size_t k = 0; k < capturedIds->size(); ++k)
        std::cout << "row " << k << " captured=" << (*capturedIds)[k]
                  << " live=" << liveIds[k]
                  << (( (*capturedIds)[k] == liveIds[k]) ? " MATCH" : " DIFFER")
                  << "\n";
}

// -------- 2. Render path (singleton app, dummy backend, live animation) -----
namespace
{
    widget::AnyWidget adderLikeRow(bq::signal::AnySignal<std::string> value)
    {
        return widget::hbox({
                    widget::label(std::move(value))
                })
            | modifier::transition(modifier::transitionLeft())
            | modifier::clip();
    }
} // namespace

// -------- 5. FULL-APP faithful captured-id "S then S" -----------------------
// Builds real adder-style rows (real buttons that capture the delegate `id`),
// mounts them through the whole pipeline in a running app, then invokes the
// two rows' real "S" actions in sequence. Reports, at the second click, whether
// findId(capturedId) finds each present row -- the exact thing the user says
// fails.
namespace
{
    struct SReport
    {
        bool ran = false;
        size_t clickId = 0;
        size_t swapState = 0;
        bool iFound = false;   // findId(this row's captured id)
        bool jFound = false;   // findId(remembered id)
        size_t liveId = 0;     // this row's live getId at click time
    };
} // namespace

TEST(SwapCrash, fullAppCapturedIdSecondSClick)
{
    Collection<std::string> items = makeItems();

    auto swapState = std::make_shared<size_t>(0);
    auto actions = std::make_shared<std::vector<std::pair<size_t,
        std::function<void()>>>>();
    auto report = std::make_shared<SReport>();

    auto widgets = dataBind<std::string>(
            dataSourceFromCollection(items),
            [items, swapState, actions, report]
            (bq::signal::AnySignal<std::string> value, size_t id) mutable
                    -> widget::AnyWidget
            {
                // The exact "S" handler adder.cpp builds, but instrumented and
                // stashed so the test can invoke it after a full mount.
                std::function<void()> sAction =
                    [items, id, swapState, report]() mutable
                    {
                        if (*swapState == 0)
                        {
                            *swapState = id;
                        }
                        else
                        {
                            auto range = items.rangeLock();
                            auto i = range.findId(id);
                            auto j = range.findId(*swapState);

                            report->ran = true;
                            report->clickId = id;
                            report->swapState = *swapState;
                            report->iFound = i != range.end();
                            report->jFound = j != range.end();
                            report->liveId = (range.size() > 0)
                                ? range.begin().getId() : 0;

                            if (report->iFound && report->jFound)
                                range.swap(i, j);   // guarded so test survives
                            *swapState = 0;
                        }
                    };

                // de-dup: keep one action per captured id (the delegate may be
                // evaluated in more than one context).
                bool seen = false;
                for (auto const& a : *actions)
                    if (a.first == id) { seen = true; break; }
                if (!seen)
                    actions->push_back({ id, sAction });

                return widget::hbox({
                        widget::button("S", bq::signal::constant(sAction)),
                        widget::label(std::move(value))
                    })
                    | modifier::transition(modifier::transitionLeft())
                    | modifier::clip();
            });

    btl::RunLoop loop;
    App app = bqui::app();
    app.platform(ase::makeDummyPlatform(loop));

    Window w = window(bq::signal::constant<std::string>("swap"));
    app.addWindow(w, widget::vbox(std::move(widgets)));

    int frames = 0;
    auto step = bq::signal::makeInput(0);

    std::vector<size_t> capturedOrder;
    std::vector<size_t> liveAtClick;

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 3:
                    // Fully mounted now: capture the id order and the live ids.
                    {
                        for (auto const& a : *actions)
                            capturedOrder.push_back(a.first);
                        liveAtClick = idsOf(items);
                    }
                    break;

                case 4:
                    // First "S" on row 0 (remembers its captured id).
                    if (capturedOrder.size() >= 3)
                    {
                        auto find = [&](size_t id) -> std::function<void()>
                        {
                            for (auto const& a : *actions)
                                if (a.first == id) return a.second;
                            return {};
                        };
                        find(capturedOrder[0])();
                    }
                    break;

                case 5:
                    // Second "S" on row 2 (performs the swap / diagnosis).
                    if (capturedOrder.size() >= 3)
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        auto find = [&](size_t id) -> std::function<void()>
                        {
                            for (auto const& aa : *actions)
                                if (aa.first == id) return aa.second;
                            return {};
                        };
                        find(capturedOrder[2])();
                    }
                    break;

                case 12:
                    w.close();
                    return false;

                default:
                    break;
                }

                step.handle.set(i + 1);
                return true;
            });

    int result = app.run(running);

    std::cout << "captured ids: ";
    for (auto id : capturedOrder) std::cout << id << " ";
    std::cout << "\nlive ids   : ";
    for (auto id : liveAtClick) std::cout << id << " ";
    std::cout << "\nreport: ran=" << report->ran
              << " clickId=" << report->clickId
              << " swapState=" << report->swapState
              << " iFound=" << report->iFound
              << " jFound=" << report->jFound << "\n";

    EXPECT_LT(frames, maxFrames);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(report->ran) << "second S action never ran";
    EXPECT_TRUE(report->iFound) << "own-row captured id NOT found";
    EXPECT_TRUE(report->jFound) << "remembered captured id NOT found";
}

void runRenderPath(bool doSwap)
{
    Collection<std::string> items = makeItems();
    auto ids = idsOf(items);
    size_t idA = ids[0];
    size_t idB = ids[2];

    auto widgets = dataBind<std::string>(
            dataSourceFromCollection(items),
            [](bq::signal::AnySignal<std::string> value, size_t) -> widget::AnyWidget
            {
                return adderLikeRow(std::move(value));
            });

    btl::RunLoop loop;

    App app = bqui::app();
    app.platform(ase::makeDummyPlatform(loop));

    Window w = window(bq::signal::constant<std::string>("swap"));
    app.addWindow(w, widget::vbox(std::move(widgets)));

    int frames = 0;
    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 0:
                    // Initial sync mounts the window on the next frame.
                    break;

                case 2:
                    // Second "S" click: swap two DIFFERENT rows under animation,
                    // exactly as adder.cpp does (no end() check).
                    if (doSwap)
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        auto range = items.rangeLock();
                        auto ii = range.findId(idB);
                        auto jj = range.findId(idA);
                        range.swap(ii, jj);
                    }
                    break;

                case 12:
                    // Let ~10 frames render the transition, then stop.
                    w.close();
                    return false;

                default:
                    break;
                }

                step.handle.set(i + 1);
                return true;
            });

    int result = app.run(running);

    EXPECT_LT(frames, maxFrames);
    EXPECT_EQ(0, result);
}

// withAnimation + NOTHING else, repeatedly, in the running app -- the exact
// work the FIRST "S" click does (minus *swapState=id). Isolates the
// animation/transaction/render path from anything collection-specific. On the
// single-threaded dummy backend this exercises AnimationGuard::makeTransaction
// over the live window impls and the transacted render tree each frame.
TEST(SwapCrash, renderPathWithAnimationOnlyRepeated)
{
    auto widgets = std::vector<widget::AnyWidget>{};

    Collection<std::string> items = makeItems();
    auto db = dataBind<std::string>(
            dataSourceFromCollection(items),
            [](bq::signal::AnySignal<std::string> value, size_t) -> widget::AnyWidget
            {
                return adderLikeRow(std::move(value));
            });

    btl::RunLoop loop;
    App app = bqui::app();
    app.platform(ase::makeDummyPlatform(loop));

    Window w = window(bq::signal::constant<std::string>("anim"));
    app.addWindow(w, widget::vbox(std::move(db)));

    int frames = 0;
    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                if (i >= 2 && i <= 8)
                {
                    // Pure first-"S" work: create an animation guard, nothing else.
                    auto a = withAnimation(0.3f, avg::curve::linear);
                }
                else if (i == 12)
                {
                    w.close();
                    return false;
                }

                step.handle.set(i + 1);
                return true;
            });

    int result = app.run(running);
    EXPECT_LT(frames, maxFrames);
    EXPECT_EQ(0, result);
}

TEST(SwapCrash, renderPathAnimatedTwoRowSwap)
{
    runRenderPath(true);
}

TEST(SwapCrash, renderPathNoSwapControl)
{
    runRenderPath(false);
}
