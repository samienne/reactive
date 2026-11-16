#include <ase/commandbuffer.h>
#include <ase/dummyplatform.h>
#include <ase/dummyrendercontext.h>
#include <ase/offscreenwindow.h>
#include <ase/platform.h>
#include <ase/readback.h>
#include <ase/rendercommand.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/renderqueueimpl.h>
#include <ase/window.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace
{
    class FakeReadbackQueue : public ase::RenderQueueImpl
    {
    public:
        void flush() override
        {
        }

        void finish() override
        {
        }

        void submit(ase::CommandBuffer&& commandBuffer) override
        {
            for (auto& command : commandBuffer)
            {
                if (auto* fence = std::get_if<ase::FenceCommand>(&command))
                {
                    fence->promise.set();
                }
                else if (auto* readback =
                        std::get_if<ase::ReadbackCommand>(&command))
                {
                    ++readbacks;

                    ase::ReadbackImage image;
                    image.size = readback->size;
                    image.pixels.assign(
                            static_cast<size_t>(readback->size[0])
                            * static_cast<size_t>(readback->size[1]) * 4u,
                            0x7f);

                    readback->callback(std::move(image));
                }
            }
        }

        int readbacks = 0;
    };

    class FakeReadbackContext : public ase::DummyRenderContext
    {
    public:
        FakeReadbackContext() :
            ase::DummyRenderContext(nullptr)
        {
        }

        std::shared_ptr<ase::RenderQueueImpl> getMainRenderQueue() override
        {
            return queue;
        }

        bool canReadback() const override
        {
            return true;
        }

        std::shared_ptr<FakeReadbackQueue> queue =
            std::make_shared<FakeReadbackQueue>();
    };

    struct Captures
    {
        std::vector<ase::CaptureResult> results;

        ase::CaptureCallback callback()
        {
            return [this](ase::CaptureResult result)
            {
                results.push_back(std::move(result));
            };
        }
    };

    ase::Vector2i const kSize(8, 4);
} // anonymous namespace

TEST(windowCapture, dummyContextCannotReadBack)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();

    EXPECT_FALSE(context.canReadback());
}

TEST(windowCapture, dummyWindowsCannotCapture)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();

    auto headless = platform.makeWindow(context, kSize, true);
    auto onScreen = platform.makeWindow(context, kSize, false);

    ASSERT_NE(nullptr, headless.getImplOfType<ase::OffscreenWindow>());
    EXPECT_FALSE(headless.canCapture());
    EXPECT_FALSE(onScreen.canCapture());
}

TEST(windowCapture, dummyCaptureFailsUnsupportedAtOnce)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();

    for (bool headless : { true, false })
    {
        auto window = platform.makeWindow(context, kSize, headless);

        Captures captures;
        window.captureNextFrame(captures.callback());

        ASSERT_EQ(1u, captures.results.size());
        ASSERT_FALSE(captures.results[0].valid());
        EXPECT_EQ(ase::ReadbackError::Unsupported, captures.results[0].err());

        window.present();
        EXPECT_EQ(1u, captures.results.size());
    }
}

TEST(windowCapture, dummyQueueFailsReadbackUnsupported)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, kSize, true);

    std::vector<ase::ReadbackResult> results;

    ase::CommandBuffer commandBuffer;
    commandBuffer.pushReadback(window.getDefaultFramebuffer(), kSize,
            [&](ase::ReadbackResult result)
            {
                results.push_back(std::move(result));
            });
    context.getMainRenderQueue().submit(std::move(commandBuffer));

    ASSERT_EQ(1u, results.size());
    ASSERT_FALSE(results[0].valid());
    EXPECT_EQ(ase::ReadbackError::Unsupported, results[0].err());
}

TEST(windowCapture, onePresentServesEveryArmedCapture)
{
    auto contextImpl = std::make_shared<FakeReadbackContext>();
    ase::RenderContext context(contextImpl);
    ase::Window window(std::make_shared<ase::OffscreenWindow>(context, kSize));

    EXPECT_TRUE(window.canCapture());

    Captures first;
    Captures second;
    window.captureNextFrame(first.callback());
    window.captureNextFrame(second.callback());

    EXPECT_TRUE(first.results.empty());
    EXPECT_EQ(0, contextImpl->queue->readbacks);

    window.present();

    EXPECT_EQ(1, contextImpl->queue->readbacks);
    for (auto* captures : { &first, &second })
    {
        ASSERT_EQ(1u, captures->results.size());
        ASSERT_TRUE(captures->results[0].valid());

        auto const& capture = captures->results[0].value();
        EXPECT_EQ(1u, capture.presentIndex);
        EXPECT_EQ(kSize, capture.image.size);
        EXPECT_EQ(static_cast<size_t>(kSize[0] * kSize[1] * 4),
                capture.image.pixels.size());
    }

    window.present();

    EXPECT_EQ(1, contextImpl->queue->readbacks);
    EXPECT_EQ(1u, first.results.size());
    EXPECT_EQ(1u, second.results.size());
}

TEST(windowCapture, presentIndexCountsEveryPresent)
{
    auto contextImpl = std::make_shared<FakeReadbackContext>();
    ase::RenderContext context(contextImpl);
    ase::Window window(std::make_shared<ase::OffscreenWindow>(context, kSize));

    window.present();
    window.present();

    Captures captures;
    window.captureNextFrame(captures.callback());
    window.present();

    ASSERT_EQ(1u, captures.results.size());
    ASSERT_TRUE(captures.results[0].valid());
    EXPECT_EQ(3u, captures.results[0].value().presentIndex);
}

TEST(windowCapture, destroyingTheWindowCancelsAnArmedCapture)
{
    auto contextImpl = std::make_shared<FakeReadbackContext>();
    ase::RenderContext context(contextImpl);

    Captures captures;

    {
        ase::Window window(
                std::make_shared<ase::OffscreenWindow>(context, kSize));
        window.captureNextFrame(captures.callback());

        EXPECT_TRUE(captures.results.empty());
    }

    ASSERT_EQ(1u, captures.results.size());
    ASSERT_FALSE(captures.results[0].valid());
    EXPECT_EQ(ase::ReadbackError::Cancelled, captures.results[0].err());
    EXPECT_EQ(0, contextImpl->queue->readbacks);
}
