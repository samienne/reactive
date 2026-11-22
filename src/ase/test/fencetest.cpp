#include <ase/basicfence.h>
#include <ase/commandbuffer.h>
#include <ase/dummyplatform.h>
#include <ase/dummyrendercontext.h>
#include <ase/fence.h>
#include <ase/platform.h>
#include <ase/rendercommand.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/renderqueueimpl.h>
#include <ase/window.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>
#include <vector>

using namespace ase;

namespace
{
    // Guards a wait that should end at once; never part of the logic.
    constexpr std::chrono::seconds kGuard{ 10 };

    // A main queue that signals its fences only when the test releases them,
    // standing in for a GPU that has not finished.
    class HoldingQueue : public RenderQueueImpl
    {
    public:
        void flush() override {}
        void finish() override {}

        void submit(CommandBuffer&& commandBuffer) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& command : commandBuffer)
            {
                if (std::holds_alternative<FenceCommand>(command))
                    held_.push_back(std::get<FenceCommand>(command).fence);
            }
        }

        std::size_t release()
        {
            std::vector<Fence> fences;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                fences.swap(held_);
            }

            std::vector<btl::future::Promise<>> unused;
            for (auto& fence : fences)
                fence.getImpl<BasicFence>().signal(unused);

            return fences.size();
        }

    private:
        std::mutex mutex_;
        std::vector<Fence> held_;
    };

    class HoldingContext : public DummyRenderContext
    {
    public:
        HoldingContext(std::shared_ptr<PlatformImpl> platform,
                std::shared_ptr<HoldingQueue> queue) :
            DummyRenderContext(std::move(platform)),
            queue_(std::move(queue))
        {
        }

        std::shared_ptr<RenderQueueImpl> getMainRenderQueue() override
        {
            return queue_;
        }

    private:
        std::shared_ptr<HoldingQueue> queue_;
    };

    class HoldingPlatform : public DummyPlatform
    {
    public:
        HoldingPlatform(btl::RunLoop& loop,
                std::shared_ptr<HoldingQueue> queue) :
            DummyPlatform(loop),
            queue_(std::move(queue))
        {
        }

        RenderContext makeRenderContext() override
        {
            return RenderContext(std::make_shared<HoldingContext>(
                        shared_from_this(), queue_));
        }

    private:
        std::shared_ptr<HoldingQueue> queue_;
    };

    // The thread a continuation on `fence` runs on.
    std::future<std::thread::id> threadOf(Fence& fence)
    {
        auto ran = std::make_shared<std::promise<std::thread::id>>();
        auto result = ran->get_future();

        fence.whenSignaled().then([ran]
            {
                ran->set_value(std::this_thread::get_id());
            }).detach();

        return result;
    }

    void stopLoop(btl::RunLoop& loop)
    {
        loop.post([](btl::RunLoop::Controller& controller)
            {
                controller.stop();
            });
    }
} // namespace

TEST(fence, dummyFenceSignalsWhenItsCommandRuns)
{
    btl::RunLoop loop;
    auto platform = makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();

    CommandBuffer commandBuffer;
    Fence fence = commandBuffer.pushFence();
    auto first = threadOf(fence);
    auto second = threadOf(fence);

    EXPECT_FALSE(fence.isSignaled());

    context.getMainRenderQueue().submit(std::move(commandBuffer));

    EXPECT_TRUE(fence.isSignaled());

    ASSERT_EQ(std::future_status::ready, first.wait_for(kGuard));
    ASSERT_EQ(std::future_status::ready, second.wait_for(kGuard));
    EXPECT_NE(std::this_thread::get_id(), first.get());
    EXPECT_NE(std::this_thread::get_id(), second.get());

    EXPECT_TRUE(fence.whenSignaled().ready());
}

TEST(fence, unsubmittedFenceFailsWithBrokenPromise)
{
    std::optional<Fence> fence;
    std::exception_ptr failure;

    {
        CommandBuffer commandBuffer;
        fence.emplace(commandBuffer.pushFence());
        fence->whenSignaled().onFailure([&failure](std::exception_ptr err)
            {
                failure = std::move(err);
            }).detach();
    }

    EXPECT_FALSE(fence->isSignaled());
    EXPECT_FALSE(failure);

    fence.reset();

    ASSERT_TRUE(failure);
    EXPECT_THROW(std::rethrow_exception(failure), btl::future::BrokenPromise);
}

TEST(fence, drainKeepsOrderAcrossBatches)
{
    auto drain = std::make_shared<FenceDrain>();

    std::mutex mutex;
    std::vector<int> order;
    std::vector<std::thread::id> threads;

    std::promise<void> secondPosted;
    auto secondPostedFuture = secondPosted.get_future().share();
    std::promise<void> done;
    auto doneFuture = done.get_future();

    auto make = [&](int index)
    {
        auto control = std::make_shared<btl::future::FutureControl<>>();
        btl::future::Promise<> promise(control);

        btl::future::Future<>(std::move(control)).then(
            [&, index, secondPostedFuture]
            {
                // Hold the drain in the first continuation until the second
                // batch has been posted, so that batch is appended to it.
                if (index == 0)
                    secondPostedFuture.wait_for(kGuard);

                std::lock_guard<std::mutex> lock(mutex);
                order.push_back(index);
                threads.push_back(std::this_thread::get_id());
                if (order.size() == 4)
                    done.set_value();
            }).detach();

        return promise;
    };

    std::vector<btl::future::Promise<>> first;
    first.push_back(make(0));
    first.push_back(make(1));

    std::vector<btl::future::Promise<>> second;
    second.push_back(make(2));
    second.push_back(make(3));

    drain->post(std::move(first));
    drain->post(std::move(second));
    secondPosted.set_value();

    ASSERT_EQ(std::future_status::ready, doneFuture.wait_for(kGuard));

    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ((std::vector<int>{ 0, 1, 2, 3 }), order);
    for (auto id : threads)
    {
        EXPECT_NE(std::this_thread::get_id(), id);
        EXPECT_EQ(threads.front(), id);
    }
}

TEST(fence, windowIsHeldBackUntilItsFencesSignal)
{
    // Long enough for several frames at the dummy's ~60 fps cadence.
    constexpr std::chrono::milliseconds kHeld{ 100 };

    btl::RunLoop loop;
    auto queue = std::make_shared<HoldingQueue>();
    Platform platform(std::make_shared<HoldingPlatform>(loop, queue));
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, Vector2i(200, 100), false);

    int frames = 0;
    int framesAtRelease = 0;
    std::size_t released = 0;
    bool timedOut = false;
    btl::RunLoop::Timer heldTimer;
    btl::RunLoop::Timer guardTimer;

    auto release = [&](btl::RunLoop::Controller&)
    {
        heldTimer.detach();
        framesAtRelease = frames;

        std::thread gpu([&]
            {
                released = queue->release();
            });
        gpu.join();
    };

    window.setFrameCallback([&](Frame const&)
            -> std::optional<std::chrono::microseconds>
            {
                ++frames;
                if (frames == 2)
                {
                    loop.post([&](btl::RunLoop::Controller& controller)
                        {
                            heldTimer = controller.addTimer(kHeld, release);
                        });
                }
                else if (frames == 3)
                {
                    stopLoop(loop);
                }

                // Animating: due again at once.
                return std::chrono::microseconds(0);
            });

    loop.post([&](btl::RunLoop::Controller& controller)
        {
            guardTimer = controller.addTimer(kGuard,
                [&](btl::RunLoop::Controller& c)
                {
                    guardTimer.detach();
                    timedOut = true;
                    c.stop();
                });
        });

    platform.run([](Frame const&) { return true; });

    EXPECT_FALSE(timedOut);
    EXPECT_EQ(2, framesAtRelease);
    EXPECT_EQ(2u, released);
    EXPECT_EQ(3, frames);
}
