#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/withanimation.h>
#include <bqui/widget/label.h>
#include <bqui/widget/introspection.h>
#include <bqui/widget/vbox.h>
#include <bqui/modifier/transition.h>

#include <bq/signal/collection.h>
#include <bq/signal/collectionsignal.h>
#include <bq/signal/input.h>
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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <thread>
#include <utility>

using namespace bqui;
using namespace bqui::widget;

namespace
{
    // A ~60fps step. The exact value is immaterial to these tests, which assert
    // the run completes cleanly, not its timing.
    constexpr std::chrono::microseconds kFrameStep{ 16667 };

    void runScript(AnyWidget content,
            std::function<void(App&, ase::PauseToken&)> script)
    {
        btl::RunLoop loop;
        auto platform = ase::makeDummyPlatform(loop);

        App app;
        app.platform(platform);
        app.addWindow(
                window(bq::signal::constant<std::string>("Test")),
                std::move(content));

        loop.post([&](btl::RunLoop::Controller& controller)
                {
                    auto token = platform.pause();
                    script(app, token);
                    controller.stop();
                });

        app.run();
    }

    std::uint64_t stepUntilNoFrame(App& app, ase::PauseToken& token)
    {
        auto frames = test::FrameDriver::windowFrames(app, 0);
        for (int i = 0; i < 100; ++i)
        {
            token.step(kFrameStep);
            auto now = test::FrameDriver::windowFrames(app, 0);
            if (now == frames)
                return now;
            frames = now;
        }
        ADD_FAILURE() << "the window never stopped drawing";
        return frames;
    }
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

TEST(headlessApp, requestFrameDuringAFrameIsNotLost)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, ase::Vector2i(200, 100), false);

    int frames = 0;
    window.setFrameCallback([&](ase::Frame const&)
            -> std::optional<std::chrono::microseconds>
            {
                if (++frames == 1)
                    window.requestFrame();
                return std::nullopt;
            });

    loop.post([&](btl::RunLoop::Controller& controller)
            {
                auto token = platform.pause();
                token.step(kFrameStep);
                token.step(kFrameStep);
                token.step(kFrameStep);
                controller.stop();
            });

    platform.run([](ase::Frame const&) { return true; });

    EXPECT_EQ(2, frames);
}

TEST(headlessApp, requestFrameFromAnotherThreadWakesAnIdleLoop)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, ase::Vector2i(200, 100), false);

    int frames = 0;
    window.setFrameCallback([&](ase::Frame const&)
            -> std::optional<std::chrono::microseconds>
            {
                ++frames;
                return std::nullopt;
            });

    std::promise<void> idle;
    std::promise<void> finished;
    std::thread requester([&]
            {
                idle.get_future().wait();
                window.requestFrame();

                if (finished.get_future().wait_for(std::chrono::seconds(10))
                        == std::future_status::timeout)
                {
                    loop.post([](btl::RunLoop::Controller& controller)
                            {
                                controller.stop();
                            });
                }
            });

    bool idleSignalled = false;
    platform.run([&](ase::Frame const&)
            {
                // The first tick after the quiescing frame schedules nothing
                // more; a task posted from it runs once the loop has gone idle.
                if (frames == 1 && !idleSignalled)
                {
                    idleSignalled = true;
                    loop.post([&](btl::RunLoop::Controller&)
                            {
                                idle.set_value();
                            });
                }
                return frames < 2;
            });

    finished.set_value();
    requester.join();

    EXPECT_EQ(2, frames);
}

TEST(headlessApp, requestFrameCutsALongWaitShort)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, ase::Vector2i(200, 100), false);

    btl::RunLoop::Timer request;
    std::chrono::steady_clock::time_point firstFrame;
    std::chrono::steady_clock::time_point secondFrame;

    int frames = 0;
    window.setFrameCallback([&](ase::Frame const&)
            -> std::optional<std::chrono::microseconds>
            {
                if (++frames != 1)
                {
                    secondFrame = std::chrono::steady_clock::now();
                    return std::nullopt;
                }

                firstFrame = std::chrono::steady_clock::now();
                loop.post([&](btl::RunLoop::Controller& controller)
                        {
                            request = controller.addTimer(
                                    std::chrono::milliseconds(100),
                                    [&](btl::RunLoop::Controller&)
                                    {
                                        window.requestFrame();
                                    });
                        });
                return std::chrono::seconds(5);
            });

    platform.run([&](ase::Frame const&) { return frames < 2; });

    ASSERT_EQ(2, frames);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(
                secondFrame - firstFrame).count(), 2000);
}

TEST(headlessApp, aLongWaitEndsOnTime)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, ase::Vector2i(200, 100), false);

    constexpr std::chrono::milliseconds kWait{ 600 };

    std::chrono::steady_clock::time_point firstFrame;
    std::chrono::steady_clock::time_point secondFrame;

    int frames = 0;
    window.setFrameCallback([&](ase::Frame const&)
            -> std::optional<std::chrono::microseconds>
            {
                if (++frames != 1)
                {
                    secondFrame = std::chrono::steady_clock::now();
                    return std::nullopt;
                }

                firstFrame = std::chrono::steady_clock::now();
                return kWait;
            });

    platform.run([&](ase::Frame const&) { return frames < 2; });

    auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
            secondFrame - firstFrame);

    ASSERT_EQ(2, frames);
    EXPECT_GE(waited.count(), kWait.count());
    EXPECT_LT(waited.count(), 2000);
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

TEST(headlessApp, oneChangeDrawsOneFrame)
{
    auto text = bq::signal::makeInput<std::string>("a");

    std::uint64_t idleFrames = 0;
    std::uint64_t frames = 0;

    runScript(label(text.signal), [&](App& app, ase::PauseToken& token)
            {
                idleFrames = stepUntilNoFrame(app, token);

                text.handle.set("b");
                for (int i = 0; i < 10; ++i)
                    token.step(kFrameStep);

                frames = test::FrameDriver::windowFrames(app, 0);
            });

    EXPECT_EQ(idleFrames + 1, frames);
}

TEST(headlessApp, changeInsideWithAnimationIsDrawn)
{
    auto text = bq::signal::makeInput<std::string>("a");

    std::uint64_t idleFrames = 0;
    std::uint64_t frames = 0;
    std::uint64_t settledFrames = 0;

    runScript(label(text.signal), [&](App& app, ase::PauseToken& token)
            {
                idleFrames = stepUntilNoFrame(app, token);

                {
                    auto guard = app.withAnimation(avg::AnimationOptions{
                            std::chrono::milliseconds(100),
                            avg::curve::linear });
                    text.handle.set("b");
                }

                frames = stepUntilNoFrame(app, token);

                for (int i = 0; i < 10; ++i)
                    token.step(kFrameStep);

                settledFrames = test::FrameDriver::windowFrames(app, 0);
            });

    EXPECT_LT(idleFrames, frames);
    EXPECT_EQ(frames, settledFrames);
}

namespace
{
    // Shows two rows, removes one under an animation of the given duration,
    // and reports whether the removed row is shown before and after.
    std::pair<bool, bool> removeRow(std::chrono::milliseconds duration)
    {
        bq::signal::Collection<std::string> items;
        {
            auto transaction = items.write();
            transaction.pushBack("stays");
            transaction.pushBack("leaves");
        }

        auto rows = bq::signal::forEach(items,
                [](bq::signal::AnySignal<std::string> text, std::uint64_t)
                    -> AnyWidget
                {
                    return label(std::move(text))
                        | modifier::transition(modifier::transitionLeft());
                });

        bool shownBefore = false;
        bool shownAfter = true;
        float x = 0.0f;
        float y = 0.0f;

        runScript(vbox(std::move(rows)),
                [&](App& app, ase::PauseToken& token)
                {
                    stepUntilNoFrame(app, token);
                    shownBefore = test::WindowInput::findText(app, 0,
                            "leaves", x, y);

                    {
                        auto guard = app.withAnimation(avg::AnimationOptions{
                                duration,
                                avg::curve::linear });
                        auto transaction = items.write();
                        transaction.erase(transaction.items().begin() + 1);
                    }

                    stepUntilNoFrame(app, token);
                    shownAfter = test::WindowInput::findText(app, 0,
                            "leaves", x, y);
                });

        return { shownBefore, shownAfter };
    }
} // namespace

TEST(headlessApp, leavingNodeIsRemovedWhenItsTransitionEnds)
{
    auto [shownBefore, shownAfter] = removeRow(std::chrono::milliseconds(100));

    EXPECT_TRUE(shownBefore);
    EXPECT_FALSE(shownAfter);
}

TEST(headlessApp, leavingNodeWithAZeroDurationIsRemoved)
{
    auto [shownBefore, shownAfter] = removeRow(std::chrono::milliseconds(0));

    EXPECT_TRUE(shownBefore);
    EXPECT_FALSE(shownAfter);
}

TEST(headlessApp, aWakeFiredAfterTheWindowIsGoneIsHarmless)
{
    auto text = bq::signal::makeInput<std::string>("a");

    // Observes the input before the window does, so a set reaches this
    // callback first and holds the window's wake until the window is gone.
    auto blocker = bq::signal::makeSignalContext(text.signal);
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    std::atomic<bool> blocking{ false };
    blocker.observe([&, released]
            {
                if (blocking.exchange(false))
                {
                    entered.set_value();
                    released.wait();
                }
            });

    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);

    App app;
    app.platform(platform);
    Window observing = window(bq::signal::constant<std::string>("Observing"));
    app.addWindow(observing, label(text.signal));
    app.addWindow(window(bq::signal::constant<std::string>("Other")),
            label("Other"));

    std::size_t windowsAfterRemove = 0;

    loop.post([&](btl::RunLoop::Controller& controller)
            {
                auto token = platform.pause();
                for (int i = 0; i < 3; ++i)
                    token.step(kFrameStep);

                blocking = true;
                std::thread setter([&]
                        {
                            text.handle.set("b");
                        });
                entered.get_future().wait();

                app.removeWindow(observing.getId());
                for (int i = 0; i < 3; ++i)
                    token.step(kFrameStep);
                windowsAfterRemove = test::WindowInput::count(app);

                release.set_value();
                setter.join();

                controller.stop();
            });

    app.run();

    EXPECT_EQ(1u, windowsAfterRemove);
}
