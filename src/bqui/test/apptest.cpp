#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/widget.h>
#include <bqui/modifier/onclick.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <btl/uniqueid.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace bqui;

// Each test builds its own App rather than calling app(), whose App shares one
// AppDeferred with every other caller — two tests would then run the same
// window collection.
//
// The app's window collection is imperative: addWindow/removeWindow/close reach
// it directly, and the run loop mounts one WindowImpl per window to it each
// frame. Two things are observable from outside without any App-level hook:
//
//   - A window's title is read once when its impl mounts, when the impl builds
//     its title SignalContext; every later read is cached, and the dummy
//     platform runs no window frames that could invalidate it. Counting reads
//     of a title therefore counts mountings, and a title read is equally the
//     report that the window has opened.
//   - A probe captured in a window's *widget* lives exactly as long as its
//     mounted impl does: the widget is the impl's, torn down when the window
//     leaves the collection. A window's persistent data does not hold it, so
//     the probe expiring is the impl being torn down — the observable proof of
//     an unmount, and, when the widget captured the owning Window, of that
//     capture not leaking.

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

    // A widget whose mounted instance retains the probe: the click handler it
    // carries holds it, so the probe lives as long as the impl and expires when
    // the window unmounts.
    widget::AnyWidget probeWidget(Probe probe)
    {
        return widget::makeWidget()
            | modifier::onClick(0, bq::signal::constant(
                        std::function<void()>([probe]() {})));
    }

    Window makeWindow(std::string title, Opens& opens)
    {
        return window(
                bq::signal::constant(std::move(title)).map(countReads(opens)));
    }

    // The dummy platform's loop has no bound of its own, so a regression that
    // stopped a script advancing would hang rather than fail.
    constexpr int maxFrames = 100;

    // A title that reports the window opening. The title is read once, when the
    // impl builds its title SignalContext, which is the app loop mounting the
    // window.
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

// A window added with its widget mounts and opens: its title is read once, and
// it is in the collection. The script drives itself through a caller-supplied
// 'running' signal, which also controls the loop.
TEST(App, addWindowMountsAndOpens)
{
    Opens opens;

    App app;

    Window w = makeWindow("only", opens);
    app.addWindow(w, probeWidget(std::make_shared<int>(0)));

    int frames = 0;
    bool openWhileRunning = false;

    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 0:
                    // Runs when the context is built, before the initial sync
                    // mounts the window; nothing to observe yet.
                    break;

                case 1:
                    // The initial sync has mounted the window by now.
                    openWhileRunning = opens["only"] == 1
                        && app.getWindows().size() == 1u;
                    break;

                default:
                    return false;
                }

                step.handle.set(i + 1);

                return true;
            });

    app.run(running);

    EXPECT_LT(frames, maxFrames);
    EXPECT_TRUE(openWhileRunning);
    EXPECT_EQ(opens, (Opens{ { "only", 1 } }));
}

// The crucial cycle-safety case. A window's widget is a close button that
// captures the *owning* Window and closes through it. Because the widget lives
// in the app-owned impl and the Window points back at the app only weakly, this
// closes no cycle: after the window is closed the impl is torn down, its widget
// with it, and the probe the widget held expires. A retain cycle would keep the
// impl alive and the probe would never expire.
TEST(App, aCloseButtonCapturingItsOwnWindowDoesNotLeak)
{
    App app;

    std::weak_ptr<int> probe;

    Window w = window(bq::signal::constant<std::string>("cycle"));

    {
        Probe held = std::make_shared<int>(0);
        probe = held;

        // The widget captures the owning window (to close it) and the probe.
        app.addWindow(w, widget::makeWidget()
                | modifier::onClick(0, bq::signal::constant(
                        std::function<void()>([w, held]() { w.close(); }))));
    }

    int frames = 0;
    bool tornDown = false;

    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 0:
                    // Built before the initial sync mounts the window; the
                    // widget (and its probe) waits in the pending queue.
                    break;

                case 1:
                    // Mounted now; the probe is held by the window's widget.
                    EXPECT_FALSE(probe.expired());
                    break;

                case 2:
                    // Close through the captured owning handle.
                    w.close();
                    break;

                case 3:
                    // Let the frame that removed it tear its impl down.
                    break;

                case 4:
                    tornDown = probe.expired();
                    break;

                default:
                    return false;
                }

                step.handle.set(i + 1);

                return true;
            });

    app.run(running);

    EXPECT_LT(frames, maxFrames);

    // The impl — and the widget that captured the owning window — is gone, so
    // the probe it held has expired: the capture did not leak the window.
    EXPECT_TRUE(tornDown);
    EXPECT_TRUE(probe.expired());
    EXPECT_TRUE(app.getWindows().empty());
}

// Removing a window during the run tears its impl down there and then, while
// the window's own data — held by a Window the caller kept — lives on and still
// reads its props.
TEST(App, removeDuringRunTearsDownButDataPersists)
{
    App app;

    std::weak_ptr<int> probe;

    Window w = window(bq::signal::constant<std::string>("kept"));

    {
        Probe held = std::make_shared<int>(0);
        probe = held;
        app.addWindow(w, probeWidget(held));
    }

    int frames = 0;
    bool aliveWhileOpen = false;
    bool goneAfterClose = false;

    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 0:
                    // Built before the initial sync mounts the window.
                    break;

                case 1:
                    aliveWhileOpen = !probe.expired();
                    app.removeWindow(w.getId());
                    break;

                case 2:
                    break;

                case 3:
                    goneAfterClose = probe.expired();
                    break;

                default:
                    return false;
                }

                step.handle.set(i + 1);

                return true;
            });

    app.run(running);

    EXPECT_LT(frames, maxFrames);
    EXPECT_TRUE(aliveWhileOpen);
    EXPECT_TRUE(goneAfterClose);
    EXPECT_TRUE(app.getWindows().empty());

    // The widget is gone, but the window's data is not: the Window still reads
    // its title.
    EXPECT_EQ("kept", bq::signal::makeSignalContext(w.getTitle())
            .evaluate<0>().get<0>());
}

// Adding the same window again after it was removed remounts it with a fresh
// widget; the old widget is torn down and the new one is what the window runs.
TEST(App, reAddAfterRemoveRemountsWithFreshWidget)
{
    App app;

    std::weak_ptr<int> first;
    std::weak_ptr<int> second;

    Window w = window(bq::signal::constant<std::string>("remount"));

    {
        Probe held = std::make_shared<int>(0);
        first = held;
        app.addWindow(w, probeWidget(held));
    }

    int frames = 0;
    bool firstGone = false;
    bool secondAlive = false;

    auto step = bq::signal::makeInput(0);

    auto running = step.signal.map(
            [&](int i) -> bool
            {
                if (++frames > maxFrames)
                    return false;

                switch (i)
                {
                case 0:
                    // Built before the initial sync mounts the first widget.
                    break;

                case 1:
                    // The first widget is mounted; remove it to unmount.
                    app.removeWindow(w.getId());
                    break;

                case 2:
                    // The old impl is torn down; re-add with a fresh widget.
                    {
                        Probe held = std::make_shared<int>(0);
                        second = held;
                        app.addWindow(w, probeWidget(held));
                    }
                    break;

                case 3:
                    // Let the re-add mount.
                    break;

                case 4:
                    firstGone = first.expired();
                    secondAlive = !second.expired();
                    break;

                case 5:
                    app.removeWindow(w.getId());
                    break;

                default:
                    return false;
                }

                step.handle.set(i + 1);

                return true;
            });

    app.run(running);

    EXPECT_LT(frames, maxFrames);
    EXPECT_TRUE(firstGone);
    EXPECT_TRUE(secondAlive);
    EXPECT_TRUE(first.expired());
    EXPECT_TRUE(second.expired());
    EXPECT_TRUE(app.getWindows().empty());
}

// A window that survives an edit keeps the impl it already had — its title is
// not read twice — and a window that is removed is torn down there and then,
// while the survivor lives on.
TEST(App, windowsOpenAndCloseImperatively)
{
    Opens opens;
    std::vector<std::weak_ptr<int>> probes;

    App app;

    std::vector<btl::UniqueId> ids;
    auto open = [&](std::string title)
    {
        Probe held = std::make_shared<int>(0);
        probes.push_back(held);

        Window w = makeWindow(std::move(title), opens);
        ids.push_back(w.getId());
        app.addWindow(w, probeWidget(held));
    };

    open("first");

    int frames = 0;
    bool survivorOutlivedTheDeparted = false;

    auto step = bq::signal::makeInput(0);

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
                    // Let the frame that removed "first" tear its impl down.
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

    ASSERT_EQ(probes.size(), 2u);
    EXPECT_TRUE(probes[0].expired());
    EXPECT_TRUE(probes[1].expired());
    EXPECT_TRUE(app.getWindows().empty());
}

// Windows added before the run all mount once, and all are torn down when the
// app that owns them is destroyed.
TEST(App, addedWindowsAllOpenOnce)
{
    Opens opens;
    std::vector<std::weak_ptr<int>> probes;

    int frames = 0;

    {
        App app;

        for (auto title : { "a", "b", "c" })
        {
            Probe held = std::make_shared<int>(0);
            probes.push_back(held);
            app.addWindow(makeWindow(title, opens), probeWidget(held));
        }

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

    // The app is destroyed with its three windows still mounted, which releases
    // their impls and the widgets they held.
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

    Window first = window(reportOpen("first", firstOpened));
    Window second = window(bq::signal::constant<std::string>("second"));
    Window later = window(reportOpen("later", laterOpened));

    app.addWindow(first, widget::makeWidget());
    app.addWindow(second, widget::makeWidget());

    std::thread closer([&]()
        {
            if (waitFor(firstOpened))
            {
                first.close();
                app.addWindow(later, widget::makeWidget());
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
