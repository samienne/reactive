#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/widget.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace bqui;

// Each test builds its own App rather than calling app(), whose App shares one
// AppDeferred with every other caller — two tests would then run the same
// window list.
//
// The app's window collection is imperative: addWindow/removeWindow/close reach
// it directly, and the run loop syncs one WindowGlue per window to it each
// frame. Nothing about a window is observable through App directly beyond
// getWindows(), so these tests watch the windows themselves. Two things are
// observable from outside without any hook:
//
//   - A window's title is read once per window that opens, when the glue builds
//     its SignalContext; every later read is cached, and the dummy platform
//     runs no window frames that could invalidate it. Counting reads of a title
//     therefore counts openings, and a window rebuilt behind our backs counts
//     twice. A title read is equally the report that the window is open, which
//     is the only thing a test running concurrently with the app loop can wait
//     for.
//   - A probe held by a window's close callback lives exactly as long as that
//     window does — as long as the collection or a glue still holds it.

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

// A window that survives an edit keeps the glue it already had — its title is
// not read twice — and a window that is removed is torn down there and then,
// while the survivor lives on. The script drives the collection imperatively
// through a caller-supplied 'running' signal, which also controls the loop.
TEST(App, windowsOpenAndCloseImperatively)
{
    Opens opens;
    std::vector<std::weak_ptr<int>> probes;

    App app;

    // Opens a window the app owns, records a weak probe on it, and returns its
    // id so the script can remove it later. No strong Window handle is kept, so
    // a probe expires exactly when the collection and the glue let go.
    std::vector<btl::UniqueId> ids;
    auto open = [&](std::string title)
    {
        Probe probe = std::make_shared<int>(0);
        probes.push_back(probe);

        Window w = window(
                bq::signal::constant(std::move(title)).map(countReads(opens)),
                widget::makeWidget())
            .onClose([probe]() {});

        ids.push_back(w.getId());
        app.addWindow(std::move(w));
    };

    open("first");

    int frames = 0;
    bool survivorOutlivedTheDeparted = false;

    auto step = bq::signal::makeInput(0);

    // The script drives itself. A SignalContext caches each signal's value and
    // re-evaluates only on a frame the signal reports changed, so a step earns
    // the next one by advancing the input it is derived from. Step 0 runs when
    // the context is built, before the first frame, so "second" is open by the
    // time the loop seeds its glues.
    //
    // Editing the collection here is seen the same frame: the loop syncs its
    // glues to getWindows() after this running signal has updated, so a removal
    // is torn down on the frame it happens. The survivor-outlived-the-departed
    // check is still made a step later, for slack.
    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 0:
                    open("second");
                    break;

                case 1:
                    app.removeWindow(ids[0]);
                    break;

                case 2:
                    // Let the frame that removed "first" tear its glue down.
                    break;

                case 3:
                    survivorOutlivedTheDeparted = probes.size() == 2u
                        && probes[0].expired()
                        && !probes[1].expired();
                    break;

                case 4:
                    app.removeWindow(ids[1]);
                    break;

                default:
                    return false;
                }

                step.handle.set(i + 1);

                return true;
            });

    app.run(running);

    EXPECT_LT(frames, maxFrames);

    EXPECT_TRUE(survivorOutlivedTheDeparted);

    // The survivor opened when it arrived and not again when its neighbour
    // left.
    EXPECT_EQ(opens, (Opens{ { "first", 1 }, { "second", 1 } }));

    // Both were removed by the script, so both are gone by the time the loop
    // stops.
    ASSERT_EQ(probes.size(), 2u);
    EXPECT_TRUE(probes[0].expired());
    EXPECT_TRUE(probes[1].expired());
    EXPECT_TRUE(app.getWindows().empty());
}

// Windows added before the run all open once, and all are gone when the app
// that owns them is.
TEST(App, addedWindowsAllOpenOnce)
{
    Opens opens;
    std::vector<Probe> owned { std::make_shared<int>(0),
        std::make_shared<int>(0), std::make_shared<int>(0) };

    std::vector<std::weak_ptr<int>> probes { owned[0], owned[1], owned[2] };

    int frames = 0;

    {
        App app;

        app.addWindows({
                makeWindow("a", opens, owned[0]),
                makeWindow("b", opens, owned[1]),
                makeWindow("c", opens, owned[2])
                });

        owned.clear();

        auto step = bq::signal::makeInput(0);

        auto running = step.signal.map(
                [&](int i) -> bool
                {
                    if (++frames > maxFrames || i != 0)
                        return false;

                    step.handle.set(1);

                    return true;
                });

        app.run(running);

        EXPECT_LT(frames, maxFrames);

        EXPECT_EQ(opens, (Opens{ { "a", 1 }, { "b", 1 }, { "c", 1 } }));
    }

    // The app is destroyed with its three windows still open, which releases
    // them.
    for (auto const& probe : probes)
        EXPECT_TRUE(probe.expired());
}

TEST(App, runWithNoWindowsReturnsImmediately)
{
    EXPECT_EQ(0, App().run());
}

// run() with no signal runs while a window is open, so closing one of several
// removes it and only the last one stops the app. A window added while the loop
// runs opens, which is what proves the loop was still running.
TEST(App, runStopsWhenTheLastWindowCloses)
{
    std::atomic<bool> firstOpened { false };
    std::atomic<bool> laterOpened { false };

    App app;

    Window first = window(reportOpen("first", firstOpened),
            widget::makeWidget());
    Window second = window(bq::signal::constant<std::string>("second"),
            widget::makeWidget());

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
