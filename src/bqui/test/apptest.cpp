#include <bqui/app.h>
#include <bqui/window.h>

#include <bqui/widget/widget.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace bqui;

// Each test builds its own App rather than calling app(), whose App shares one
// AppDeferred with every other caller — two tests would then run the same
// window list.
//
// App reports nothing about the open windows, so these tests watch the windows
// themselves. Two things are observable from outside without any hook:
//
//   - A window's title is read once per window that opens, when the glue builds
//     its SignalContext; every later read is cached, and the dummy platform
//     runs no window frames that could invalidate it. Counting reads of a title
//     therefore counts openings, and a window rebuilt behind our backs counts
//     twice.
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
    // Editing the window list from here is the one case of doing so during the
    // frame phase that is sound: App reconciles the list before it evaluates
    // this, so a step sees what the step before it asked for and no window is
    // updated after its key left.
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

    App().windows(std::move(windows)).run(running);

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

    App().windows(std::move(windows)).run(running);

    EXPECT_LT(frames, maxFrames);

    EXPECT_EQ(opens, (Opens{ { "a", 1 }, { "b", 1 }, { "c", 1 } }));

    for (auto const& probe : probes)
        EXPECT_TRUE(probe.expired());
}
