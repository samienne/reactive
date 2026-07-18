#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/withanimation.h>
#include <bqui/widget/label.h>
#include <bqui/widget/introspection.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <ase/platform.h>
#include <ase/dummyplatform.h>
#include <ase/window.h>
#include <ase/pointerbuttonevent.h>
#include <ase/keyevent.h>

#include <gtest/gtest.h>

using namespace bqui;
using namespace bqui::widget;

namespace
{
    // A headless platform capped to a few frames, so a run is bounded and fast.
    ase::Platform makeBoundedHeadless(uint64_t frames)
    {
        auto platform = ase::makeDummyPlatform();
        platform.getImpl<ase::DummyPlatform>().setMaxFrames(frames);
        return platform;
    }
} // namespace

TEST(headlessApp, runsAndExitsWithNoWindow)
{
    // A minimal app driven by the headless backend must build, tick a bounded
    // number of frames, and return cleanly without ever opening an OS window.
    auto widget = label("Headless");

    int result = app()
        .platform(makeBoundedHeadless(5))
        .addWindow(
                window(bq::signal::constant<std::string>("Test"),
                    std::move(widget)))
        .run();

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
    auto platform = ase::makeDummyPlatform();
    auto window = platform.makeWindow(ase::Vector2i(200, 100));

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

TEST(headlessApp, isReRunnable)
{
    // Running an app must not consume the window so another app can run in the
    // same process (the loop and its tests rely on this).
    for (int i = 0; i < 2; ++i)
    {
        int result = app()
            .platform(makeBoundedHeadless(3))
            .windows({
                    window(bq::signal::constant<std::string>("Test"),
                        label("Rerun"))
                    })
            .run();

        EXPECT_EQ(0, result);
    }
}

TEST(headlessApp, withAnimationIsANoOpWithNoRunningApp)
{
    // The free withAnimation reaches the running app; with none running it must
    // be a harmless no-op, not a crash.
    withAnimation(std::chrono::milliseconds(100), avg::curve::linear,
            [] {});

    SUCCEED();
}
