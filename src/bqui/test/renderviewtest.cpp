#include <bqui/app.h>
#include <bqui/renderview.h>
#include <bqui/window.h>
#include <bqui/modifier/handlegravity.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/simplesizehint.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/renderview.h>

#include <bq/signal/constant.h>
#include <bq/signal/input.h>

#include <ase/commandbuffer.h>
#include <ase/dummyplatform.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>

#include "apptestsupport.h"

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace bqui;

namespace
{
    constexpr std::chrono::microseconds kFrameStep{ 16667 };

    struct Call
    {
        std::vector<RenderTarget> targets;
        std::chrono::microseconds time;
    };

    void runScript(widget::AnyWidget content,
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

    // setSize() would still let the view's own expanding hint fill the window.
    widget::AnyWidget sized(widget::AnyWidget widget,
            bq::signal::AnySignal<avg::Vector2f> size)
    {
        return std::move(widget)
            | modifier::setSizeHint(size.map([](avg::Vector2f size)
                        -> SizeHint
                    {
                        return simpleSizeHint(size.x(), size.y());
                    }))
            | modifier::handleGravity();
    }

    void step(ase::PauseToken& token, int frames)
    {
        for (int i = 0; i < frames; ++i)
            token.step(kFrameStep);
    }
} // anonymous namespace

TEST(renderView, callbackGetsTheViewSizedFramebuffer)
{
    std::vector<Call> calls;
    RenderHandle handle([&](RenderFrame& frame)
            {
                ase::CommandBuffer commands;
                for (auto const& target : frame.getTargets())
                    commands.pushClear(target.framebuffer, 1.0f, 0.0f, 0.0f);
                frame.getRenderContext().getMainRenderQueue().submit(
                        std::move(commands));

                calls.push_back({ frame.getTargets(), frame.getTime() });
            });

    auto view = handle.makeView();

    runScript(sized(widget::renderView(view), bq::signal::constant(
                    avg::Vector2f(64.0f, 32.0f))),
            [&](App&, ase::PauseToken& token)
            {
                step(token, 5);
            });

    ASSERT_EQ(1u, calls.size());
    ASSERT_EQ(1u, calls[0].targets.size());
    EXPECT_EQ(view, calls[0].targets[0].view);
    EXPECT_EQ(ase::Vector2i(64, 32), calls[0].targets[0].size);
    EXPECT_TRUE(calls[0].targets[0].resized);
}

TEST(renderView, resizeRecreatesTheTargets)
{
    auto size = bq::signal::makeInput(avg::Vector2f(64.0f, 32.0f));

    std::vector<Call> calls;
    RenderHandle handle([&](RenderFrame& frame)
            {
                calls.push_back({ frame.getTargets(), frame.getTime() });
            });

    runScript(sized(widget::renderView(handle.makeView()), size.signal),
            [&](App&, ase::PauseToken& token)
            {
                step(token, 5);
                size.handle.set(avg::Vector2f(20.0f, 10.0f));
                step(token, 5);
            });

    ASSERT_EQ(2u, calls.size());
    EXPECT_EQ(ase::Vector2i(20, 10), calls[1].targets[0].size);
    EXPECT_TRUE(calls[1].targets[0].resized);
    EXPECT_NE(calls[0].targets[0].framebuffer,
            calls[1].targets[0].framebuffer);
}

TEST(renderView, continuousRunsEveryFrameUntilStopped)
{
    int calls = 0;
    std::optional<RenderHandle> handle;
    handle.emplace([&](RenderFrame&) { ++calls; });
    handle->setContinuous(true);

    int whileContinuous = 0;

    runScript(sized(widget::renderView(handle->makeView()),
                bq::signal::constant(avg::Vector2f(8.0f, 8.0f))),
            [&](App& app, ase::PauseToken& token)
            {
                step(token, 10);
                whileContinuous = calls;

                auto frames = test::FrameDriver::windowFrames(app, 0);
                step(token, 3);
                EXPECT_EQ(frames + 3, test::FrameDriver::windowFrames(app, 0));

                handle->setContinuous(false);
                step(token, 3);
                int stopped = calls;
                step(token, 5);
                EXPECT_EQ(stopped, calls);
            });

    EXPECT_GE(whileContinuous, 9);
}

TEST(renderView, requestFrameRunsOneMoreCallback)
{
    int calls = 0;
    RenderHandle handle([&](RenderFrame& frame)
            {
                if (++calls == 1)
                    frame.requestFrame();
            });

    int beforeRequest = 0;

    runScript(sized(widget::renderView(handle.makeView()),
                bq::signal::constant(avg::Vector2f(8.0f, 8.0f))),
            [&](App&, ase::PauseToken& token)
            {
                step(token, 10);
                beforeRequest = calls;

                handle.requestFrame();
                step(token, 10);
            });

    EXPECT_EQ(2, beforeRequest);
    EXPECT_EQ(3, calls);
}

TEST(renderView, destroyedHandleStopsTheCallback)
{
    int calls = 0;
    std::optional<RenderHandle> handle;
    handle.emplace([&](RenderFrame&) { ++calls; });
    handle->setContinuous(true);

    runScript(widget::hbox({
                widget::renderView(handle->makeView()),
                widget::label(std::string("Beside"))
                }),
            [&](App&, ase::PauseToken& token)
            {
                step(token, 3);
                handle.reset();
                int before = calls;
                step(token, 3);
                EXPECT_EQ(before, calls);
            });

    EXPECT_GE(calls, 1);
}
