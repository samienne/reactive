#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/widget.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace bqui;

// Each test builds its own App rather than calling app(), whose App shares one
// AppDeferred with every other caller — two tests would then run the same
// window list.
//
// App::getWindows reports the app's own collection, and nothing at all about a
// caller-supplied array or about what has actually been opened, so these tests
// watch the windows themselves for the rest. Two things are observable from
// outside without any hook:
//
//   - A window's title is read once per window that opens, when the glue builds
//     its SignalContext; every later read is cached, and the dummy platform
//     runs no window frames that could invalidate it. Counting reads of a title
//     therefore counts openings, and a window rebuilt behind our backs counts
//     twice. A title read is equally the report that the window is open, which
//     is the only thing a test running concurrently with the app loop can wait
//     for.
//   - A probe held by a window's close callback lives exactly as long as that
//     window does.

namespace
{
    using Probe = std::shared_ptr<int>;
    using Opens = std::map<std::string, int>;

    auto countReads(Opens& opens)
    {
        return [&opens](std::string const& title)
        {
            ++opens[title];
            return title;
        };
    }

    Window makeWindow(std::string title, Opens& opens, Probe probe)
    {
        return window(
                bq::signal::constant(std::move(title)).map(countReads(opens)),
                widget::makeWidget())
            .onClose([probe]() {});
    }

    // The dummy platform's loop has no bound of its own, so a regression that
    // stopped a script advancing would hang rather than fail.
    constexpr int maxFrames = 100;

    // A title that reports the window opening. The title is read once, when the
    // glue builds its SignalContext, which is the app loop opening the window.
    auto reportOpen(std::string title, std::atomic<bool>& opened)
    {
        return bq::signal::constant(std::move(title)).map(
                [&opened](std::string const& text)
                {
                    opened = true;
                    return text;
                });
    }

    // Waits for the app loop to report something, and gives up rather than
    // hanging if it never does — a stopped loop has to fail the test, not wedge
    // the suite. The deadline is a backstop and not an ordering proxy: the test
    // proceeds on the flag, never on the clock.
    bool waitFor(std::atomic<bool> const& flag)
    {
        auto const deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(30);

        while (!flag)
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;

            std::this_thread::yield();
        }

        return true;
    }
} // namespace

// A window whose key stays is not rebuilt when its neighbours come and go —
// neither its delegate nor its glue runs twice — and a window whose key leaves
// is destroyed there and then.
TEST(App, windowsOpenAndCloseWithTheirKeys)
{
    auto titles = bq::signal::makeInput(std::vector<std::string>{ "first" });

    int builds = 0;
    Opens opens;
    std::vector<std::weak_ptr<int>> probes;

    auto windows = bq::signal::forEach(titles.signal,
            [](std::string const& title) { return title; },
            [&](bq::signal::AnySignal<std::string> title)
            {
                ++builds;

                Probe probe = std::make_shared<int>(0);
                probes.push_back(probe);

                return window(title.map(countReads(opens)),
                        widget::makeWidget())
                    .onClose([probe]() {});
            });

    int frames = 0;
    bool survivorOutlivedTheDeparted = false;

    auto step = bq::signal::makeInput(0);

    // The script drives itself. A SignalContext caches each signal's value and
    // re-evaluates only on a frame the signal reports changed, so a step earns
    // the next one by advancing the input it is derived from. Step 0 runs when
    // the context is built, before the first frame.
    //
    // Editing the window list from here is sound, but its effect is observed a
    // frame later: the app context evaluates this running signal each frame and
    // reconciles the glue list AFTER, so a step that removes a key does not see
    // the teardown until a subsequent frame. The step that removes "first"
    // (case 1) therefore cannot yet see its glue gone; the survivor-outlived-
    // the-departed check is made a step later (case 3), once the reconcile has
    // destroyed the departed glue on an earlier frame.
    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 0:
                    titles.handle.set(
                            std::vector<std::string>{ "first", "second" });
                    break;

                case 1:
                    titles.handle.set(std::vector<std::string>{ "second" });
                    break;

                case 2:
                    // Let the reconcile that removes "first" complete on this
                    // frame before observing its effect on the next.
                    break;

                case 3:
                    survivorOutlivedTheDeparted = probes.size() == 2u
                        && probes[0].expired()
                        && !probes[1].expired();
                    break;

                default:
                    return false;
                }

                step.handle.set(i + 1);

                return true;
            });

    App().addWindowArray(std::move(windows)).run(running);

    EXPECT_LT(frames, maxFrames);

    EXPECT_EQ(builds, 2);
    EXPECT_TRUE(survivorOutlivedTheDeparted);

    // The survivor opened when it arrived and not again when its neighbour
    // left.
    EXPECT_EQ(opens, (Opens{ { "first", 1 }, { "second", 1 } }));

    ASSERT_EQ(probes.size(), 2u);
    EXPECT_TRUE(probes[0].expired());
    EXPECT_TRUE(probes[1].expired());
}

// A braced list is a constant array: every window in it opens once and every
// one is gone when the app is.
TEST(App, staticWindowsAllOpenOnce)
{
    Opens opens;
    std::vector<Probe> owned { std::make_shared<int>(0),
        std::make_shared<int>(0), std::make_shared<int>(0) };

    std::vector<std::weak_ptr<int>> probes { owned[0], owned[1], owned[2] };

    bq::signal::ArraySignal<Window> windows = {
        makeWindow("a", opens, owned[0]),
        makeWindow("b", opens, owned[1]),
        makeWindow("c", opens, owned[2])
    };

    owned.clear();

    int frames = 0;
    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames || i != 0)
                    return false;

                step.handle.set(1);

                return true;
            });

    App().addWindowArray(std::move(windows)).run(running);

    EXPECT_LT(frames, maxFrames);

    EXPECT_EQ(opens, (Opens{ { "a", 1 }, { "b", 1 }, { "c", 1 } }));

    for (auto const& probe : probes)
        EXPECT_TRUE(probe.expired());
}

// The app's own collection and a caller-supplied array are both open at once,
// and each is closed by its own means.
TEST(App, ownedAndSuppliedWindowsCoexist)
{
    Opens opens;

    auto names = bq::signal::makeInput(
            std::vector<std::string>{ "supplied" });

    auto supplied = bq::signal::forEach(names.signal,
            [](std::string const& name) { return name; },
            [&](bq::signal::AnySignal<std::string> name)
            {
                return window(name.map(countReads(opens)),
                        widget::makeWidget());
            });

    App app;
    Window owned = makeWindow("owned", opens, std::make_shared<int>(0));

    app.addWindow(owned);
    app.addWindowArray(std::move(supplied));

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
                    // Neither means of closing reaches the other's list.
                    owned.close();
                    break;

                case 1:
                    names.handle.set(std::vector<std::string>{});
                    break;

                default:
                    return false;
                }

                step.handle.set(i + 1);

                return true;
            });

    app.run(running);

    EXPECT_LT(frames, maxFrames);
    EXPECT_EQ(opens, (Opens{ { "owned", 1 }, { "supplied", 1 } }));
    EXPECT_TRUE(app.getWindows().empty());
}

TEST(App, runWithNoWindowsReturnsImmediately)
{
    EXPECT_EQ(0, App().run());
}

// The arrays are joined once, at the start, so one added later would open
// nothing. Saying so beats a window that never appears.
TEST(App, addingAnArrayWhileRunningIsRejected)
{
    Opens opens;

    App app;
    app.addWindow(makeWindow("owned", opens, std::make_shared<int>(0)));

    bool rejected = false;
    int frames = 0;
    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames || i != 0)
                    return false;

                try
                {
                    app.addWindowArray(bq::signal::ArraySignal<Window>());
                }
                catch (std::logic_error const&)
                {
                    rejected = true;
                }

                step.handle.set(1);

                return true;
            });

    app.run(running);

    EXPECT_LT(frames, maxFrames);
    EXPECT_TRUE(rejected);
}

// run() with no signal runs while a window is open, so closing one of several
// removes it and only the last one stops the app.
TEST(App, runStopsWhenTheLastWindowCloses)
{
    std::atomic<bool> firstOpened { false };
    std::atomic<bool> laterOpened { false };

    Opens opens;

    App app;

    Window first = window(reportOpen("first", firstOpened),
            widget::makeWidget());
    Window second = makeWindow("second", opens, std::make_shared<int>(0));

    // A window added after another has closed opens only if the loop is still
    // running, which is the whole of what this test asserts. Waiting for that
    // window to open is waiting for an event the app produces, rather than for
    // a stretch of wall clock in which it might have.
    Window later = window(reportOpen("later", laterOpened),
            widget::makeWidget());

    app.addWindows({ first, second });

    std::thread closer([&]()
        {
            if (waitFor(firstOpened))
            {
                first.close();
                app.addWindow(later);
                waitFor(laterOpened);
            }

            // Closed unconditionally, and closing twice or closing a window
            // that was never added is a no-op: a loop that did not report what
            // was waited for has to fail the assertions rather than run on.
            first.close();
            second.close();
            later.close();
        });

    EXPECT_EQ(0, app.run());

    closer.join();

    EXPECT_TRUE(laterOpened);
    EXPECT_TRUE(app.getWindows().empty());
}
