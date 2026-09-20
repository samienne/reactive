#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/withanimation.h>
#include <bqui/widget/label.h>
#include <bqui/widget/introspection.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <ase/platform.h>
#include <ase/dummyplatform.h>
#include <ase/rendercontext.h>
#include <ase/window.h>
#include <ase/pointerbuttonevent.h>
#include <ase/keyevent.h>

#include "apptestsupport.h"

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <chrono>

using namespace bqui;
using namespace bqui::widget;

namespace
{
    // A ~60fps step. The exact value is immaterial to these tests, which assert
    // the run completes cleanly, not its timing.
    constexpr std::chrono::microseconds kFrameStep{ 16667 };
} // namespace

TEST(headlessApp, runsAndExitsWithNoWindow)
{
    // A minimal app driven by the headless backend must build, produce a bounded
    // number of frames, and return cleanly without ever opening an OS window.
    // Frames are stepped explicitly, so the run is bounded by construction rather
    // than by the loop's cadence.
    auto widget = label("Headless");

    btl::RunLoop loop;

    App app;
    app.platform(ase::makeDummyPlatform(loop));
    app.addWindow(
            window(bq::signal::constant<std::string>("Test")),
            std::move(widget));

    int result = test::FrameDriver::run(app, loop, 5, kFrameStep);

    EXPECT_EQ(0, result);
}

TEST(headlessApp, introspectionResolvesWithNoWindow)
{
    // Introspection is available headless: build a widget, realise it at a
    // concrete size, and read a resolved snapshot with sane window-space obbs —
    // no platform or window involved at all.
    auto widget = label("Snapshot");

    auto sig = std::move(widget)(BuildParams{})(
                bq::signal::constant(avg::Vector2f(200.0f, 100.0f)))
            .getIntrospection();

    auto node = bq::signal::makeSignalContext(std::move(sig))
        .evaluate<0>().get<0>();

    EXPECT_EQ("Label", node.role);
    EXPECT_GT(node.obb.getSize()[0], 0.0f);
    EXPECT_GT(node.obb.getSize()[1], 0.0f);
}

TEST(headlessApp, injectsEventsThroughTheAbstractWindow)
{
    // Events injected via the abstract ase::Window interface reach the window's
    // callbacks — the uniform, backend-agnostic seam (no GenericWindow in
    // sight). Exercised on the headless window here; every backend delegates
    // identically.
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, ase::Vector2i(200, 100), false);

    std::optional<ase::PointerButtonEvent> gotButton;
    window.setButtonCallback([&](ase::PointerButtonEvent const& e)
            {
                gotButton = e;
            });

    std::string gotText;
    window.setTextCallback([&](ase::TextEvent const& e)
            {
                gotText = e.getText();
            });

    window.injectPointerButtonEvent(0, 1, ase::Vector2f(20.0f, 30.0f),
            ase::ButtonState::down);
    window.injectTextEvent("hi");

    ASSERT_TRUE(gotButton.has_value());
    EXPECT_EQ(1u, gotButton->button);
    EXPECT_EQ(ase::ButtonState::down, gotButton->state);
    EXPECT_EQ(ase::Vector2f(20.0f, 30.0f), gotButton->pos);
    EXPECT_EQ("hi", gotText);
}

TEST(headlessApp, secondAppRunsInTheSameProcess)
{
    // App::run clones its windows rather than consuming them, so a second app
    // can build and run in the same process without hitting an emptied widget.
    for (int i = 0; i < 2; ++i)
    {
        btl::RunLoop loop;

        App app;
        app.platform(ase::makeDummyPlatform(loop));
        app.addWindow(
                window(bq::signal::constant<std::string>("Test")),
                label("Rerun"));

        int result = test::FrameDriver::run(app, loop, 3, kFrameStep);

        EXPECT_EQ(0, result);
    }
}

TEST(headlessApp, withAnimationIsANoOpWithNoRunningApp)
{
    // The free withAnimation reaches the singleton app; with no run active its
    // window impls are empty, so it must be a harmless no-op, not a crash.
    withAnimation(std::chrono::milliseconds(100), avg::curve::linear,
            [] {});

    SUCCEED();
}
