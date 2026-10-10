#include "purelayouttestutil.h"

#include "apptestsupport.h"
#include "windowsizing.h"

#include <bqui/app.h>
#include <bqui/window.h>

#include <ase/dummyplatform.h>

#include <btl/runloop.h>

#include <bq/signal/input.h>

#include <gtest/gtest.h>

#include <functional>
#include <string>

using namespace bqui;
using namespace bqui::widget;
using namespace bqui::test;
using namespace bq::signal;

namespace
{

Constraints band(std::optional<float> min, std::optional<float> natural,
        std::optional<float> max)
{
    Constraints c;
    c.min = min;
    c.max = max;
    if (natural)
        c.natural = BandNatural{ *natural, contentStrength() };
    return c;
}

auto heightBand(Constraints c)
{
    return [c](float) { return c; };
}

avg::Vector2i const fallback(800, 600);

// Opens @p root in @p window on the dummy platform and runs a few frames,
// calling @p each with the frame index once the window is mounted.
void runWindow(Window win, AnyWidget root,
        std::function<void(App&, int)> const& each)
{
    btl::RunLoop loop;
    App app;
    app.platform(ase::makeDummyPlatform(loop));
    app.addWindow(win, std::move(root));

    constexpr int frames = 8;
    auto step = makeInput(0);
    auto running = step.signal.map([&](int i) -> bool
        {
            if (WindowInput::count(app) > 0)
                each(app, i);

            if (i >= frames)
            {
                win.close();
                return false;
            }

            step.handle.set(i + 1);
            return true;
        });

    EXPECT_EQ(0, app.run(running));
}

Window testWindow()
{
    return window(constant<std::string>("sizing"));
}

} // namespace

TEST(WindowSizing, opensAtNatural)
{
    avg::Vector2i size = initialWindowSize(band({}, 300.2f, {}),
            heightBand(band({}, 200.0f, {})), std::nullopt, fallback);

    EXPECT_EQ(avg::Vector2i(301, 200), size);
}

TEST(WindowSizing, fallsBackPerAxisWithoutNatural)
{
    avg::Vector2i size = initialWindowSize(band({}, {}, {}),
            heightBand(band({}, 200.0f, {})), std::nullopt, fallback);

    EXPECT_EQ(avg::Vector2i(800, 200), size);
}

TEST(WindowSizing, fallbackIsLimited)
{
    avg::Vector2i size = initialWindowSize(band({}, {}, 500.0f),
            heightBand(band(700.0f, {}, {})), std::nullopt, fallback);

    EXPECT_EQ(avg::Vector2i(500, 700), size);
}

TEST(WindowSizing, explicitSizeOverridesNatural)
{
    avg::Vector2i size = initialWindowSize(band({}, 300.0f, {}),
            heightBand(band({}, 200.0f, {})), avg::Vector2f(500.0f, 400.0f),
            fallback);

    EXPECT_EQ(avg::Vector2i(500, 400), size);
}

TEST(WindowSizing, explicitSizeIsLimited)
{
    avg::Vector2i size = initialWindowSize(band(400.0f, 450.0f, {}),
            heightBand(band(50.0f, 100.0f, 150.5f)),
            avg::Vector2f(100.0f, 1000.0f), fallback);

    EXPECT_EQ(avg::Vector2i(400, 150), size);
}

TEST(WindowSizing, minWinsOverMax)
{
    avg::Vector2i size = initialWindowSize(band(300.0f, {}, 200.0f),
            heightBand(band({}, 100.0f, {})), std::nullopt, fallback);

    EXPECT_EQ(300, size[0]);
    EXPECT_FLOAT_EQ(300.0f, clampToBand(250.0f, band(300.0f, {}, 200.0f)));
}

TEST(WindowSizing, heightIsReadAtTheChosenWidth)
{
    float seenWidth = 0.0f;
    avg::Vector2i size = initialWindowSize(band({}, 250.0f, {}),
            [&seenWidth](float width)
            {
                seenWidth = width;
                return band({}, 6000.0f / width, {});
            },
            std::nullopt, fallback);

    EXPECT_FLOAT_EQ(250.0f, seenWidth);
    EXPECT_EQ(avg::Vector2i(250, 24), size);
}

TEST(WindowSizing, clampLeavesASizeInsideTheBand)
{
    EXPECT_FLOAT_EQ(120.0f, clampToBand(120.0f, band(100.0f, 50.0f, 200.0f)));
    EXPECT_FLOAT_EQ(100.0f, clampToBand(20.0f, band(100.0f, 50.0f, 200.0f)));
    EXPECT_FLOAT_EQ(200.0f, clampToBand(900.0f, band(100.0f, 50.0f, 200.0f)));
    EXPECT_FLOAT_EQ(900.0f, clampToBand(900.0f, band({}, 50.0f, {})));
}

TEST(WindowSizing, windowOpensAtRootNatural)
{
    avg::Vector2f size;
    runWindow(testWindow(),
            makeWidget() | modifier::defaultSize(avg::Vector2f(300.0f, 200.0f)),
            [&](App& app, int) { size = WindowInput::windowSize(app, 0); });

    EXPECT_EQ(avg::Vector2f(300.0f, 200.0f), size);
}

TEST(WindowSizing, windowOpensAtHeightForNaturalWidth)
{
    avg::Vector2f size;
    runWindow(testWindow(), reflowProbe(btl::makeUniqueId(), 6000.0f, 100.0f),
            [&](App& app, int) { size = WindowInput::windowSize(app, 0); });

    EXPECT_EQ(avg::Vector2f(100.0f, 60.0f), size);
}

TEST(WindowSizing, windowFallsBackWithoutNatural)
{
    avg::Vector2f size;
    runWindow(testWindow(), filler(),
            [&](App& app, int) { size = WindowInput::windowSize(app, 0); });

    EXPECT_EQ(avg::Vector2f(800.0f, 600.0f), size);
}

TEST(WindowSizing, windowOpensAtInitialSize)
{
    avg::Vector2f size;
    avg::Vector2f layout;
    runWindow(testWindow().initialSize(avg::Vector2f(500.0f, 400.0f)),
            makeWidget() | modifier::defaultSize(avg::Vector2f(300.0f, 200.0f)),
            [&](App& app, int)
            {
                size = WindowInput::windowSize(app, 0);
                layout = WindowInput::layoutSize(app, 0);
            });

    EXPECT_EQ(avg::Vector2f(500.0f, 400.0f), size);
    EXPECT_EQ(size, layout);
}

TEST(WindowSizing, initialSizeIsLimitedByTheRoot)
{
    avg::Vector2f size;
    runWindow(testWindow().initialSize(avg::Vector2f(100.0f, 1000.0f)),
            probe(btl::makeUniqueId(), { 400.0f, 450.0f, 10000.0f, 1.0f },
                { 50.0f, 100.0f, 150.0f, 1.0f }),
            [&](App& app, int) { size = WindowInput::windowSize(app, 0); });

    EXPECT_EQ(avg::Vector2f(400.0f, 150.0f), size);
}

// The OS window cannot be resized from bqui, so a min that outgrows it lays the
// root out at the min and lets it overflow the window.
TEST(WindowSizing, growingMinClampsTheLayoutSize)
{
    auto minWidth = makeInput(0.0f);
    auto minHeight = makeInput(0.0f);

    avg::Vector2f size;
    avg::Vector2f layout;
    runWindow(testWindow(),
            makeWidget()
                | modifier::defaultSize(avg::Vector2f(100.0f, 50.0f))
                | modifier::minWidth(minWidth.signal)
                | modifier::minHeight(minHeight.signal),
            [&](App& app, int i)
            {
                if (i == 2)
                {
                    minWidth.handle.set(300.0f);
                    minHeight.handle.set(120.0f);
                }
                size = WindowInput::windowSize(app, 0);
                layout = WindowInput::layoutSize(app, 0);
            });

    EXPECT_EQ(avg::Vector2f(100.0f, 50.0f), size);
    EXPECT_EQ(avg::Vector2f(300.0f, 120.0f), layout);
}

TEST(WindowSizing, laterNaturalChangeKeepsTheSize)
{
    auto natural = makeInput(avg::Vector2f(100.0f, 50.0f));

    avg::Vector2f size;
    avg::Vector2f layout;
    runWindow(testWindow(),
            makeWidget() | modifier::defaultSize(natural.signal),
            [&](App& app, int i)
            {
                if (i == 2)
                    natural.handle.set(avg::Vector2f(250.0f, 180.0f));
                size = WindowInput::windowSize(app, 0);
                layout = WindowInput::layoutSize(app, 0);
            });

    EXPECT_EQ(avg::Vector2f(100.0f, 50.0f), size);
    EXPECT_EQ(size, layout);
}
