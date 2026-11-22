#include <ase/commandbuffer.h>
#include <ase/dummyplatform.h>
#include <ase/platform.h>
#include <ase/readback.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/window.h>

#include <btl/future/promise.h>
#include <btl/runloop.h>

#include <gtest/gtest.h>

namespace
{
    ase::Vector2i const kSize(8, 4);
} // anonymous namespace

TEST(readback, dummyContextCannotReadBack)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();

    EXPECT_FALSE(context.canReadback());
}

TEST(readback, onlyAHeadlessWindowOwnsItsFramebuffer)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();

    auto headless = platform.makeWindow(context, kSize, true);
    auto onScreen = platform.makeWindow(context, kSize, false);

    EXPECT_TRUE(headless.ownsDefaultFramebuffer());
    EXPECT_FALSE(onScreen.ownsDefaultFramebuffer());
}

TEST(readback, dummyQueueFailsReadbackUnsupported)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, kSize, true);

    ase::CommandBuffer commandBuffer;
    auto image = commandBuffer.pushReadback(window.getDefaultFramebuffer());
    context.getMainRenderQueue().submit(std::move(commandBuffer));

    ASSERT_TRUE(image.ready());
    try
    {
        std::move(image).get();
        ADD_FAILURE() << "the readback produced pixels";
    }
    catch (ase::ReadbackFailure const& failure)
    {
        EXPECT_EQ(ase::ReadbackError::Unsupported, failure.error());
    }
}

TEST(readback, readbackDroppedUnrunBreaksItsPromise)
{
    btl::RunLoop loop;
    auto platform = ase::makeDummyPlatform(loop);
    auto context = platform.makeRenderContext();
    auto window = platform.makeWindow(context, kSize, true);

    auto image = [&]
    {
        ase::CommandBuffer commandBuffer;
        return commandBuffer.pushReadback(window.getDefaultFramebuffer());
    }();

    ASSERT_TRUE(image.ready());
    EXPECT_THROW(std::move(image).get(), btl::future::BrokenPromise);
}
