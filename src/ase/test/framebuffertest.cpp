#include <ase/attachmentframebuffer.h>
#include <ase/dummyplatform.h>
#include <ase/format.h>
#include <ase/framebuffer.h>
#include <ase/framebufferimpl.h>
#include <ase/platform.h>
#include <ase/renderbuffer.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>
#include <ase/window.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

using namespace ase;

namespace
{
    Vector2i const size(16, 8);

    AttachmentFramebuffer const* attachmentsOf(Framebuffer const& framebuffer)
    {
        return framebuffer.getImpl<FramebufferImpl>().asAttachments();
    }
} // anonymous namespace

TEST(framebuffer, contextFramebuffersDescribeAttachments)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    Framebuffer first = context.makeFramebuffer();
    Framebuffer second = context.makeFramebuffer();
    AttachmentFramebuffer const* firstAttachments = attachmentsOf(first);
    AttachmentFramebuffer const* secondAttachments = attachmentsOf(second);

    ASSERT_NE(nullptr, firstAttachments);
    ASSERT_NE(nullptr, secondAttachments);
    EXPECT_NE(firstAttachments->getId(), secondAttachments->getId());
}

TEST(framebuffer, everyChangeBumpsTheVersion)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    Framebuffer framebuffer = context.makeFramebuffer();
    AttachmentFramebuffer const& attachments = *attachmentsOf(framebuffer);
    uint64_t version = attachments.getVersion();

    auto expectBumped = [&]()
    {
        EXPECT_NE(version, attachments.getVersion());
        version = attachments.getVersion();
    };

    framebuffer.setColorTarget(0, context.makeTexture(size, FORMAT_SRGBA));
    expectBumped();
    framebuffer.setColorTarget(1, context.makeTexture(size, FORMAT_SRGBA));
    expectBumped();
    framebuffer.unsetColorTarget(1);
    expectBumped();
    framebuffer.setDepthTarget(context.makeRenderbuffer(size, FORMAT_DEPTH16));
    expectBumped();
    framebuffer.unsetDepthTarget();
    expectBumped();
    framebuffer.unsetColorTarget(0);
    expectBumped();
}

TEST(framebuffer, snapshotSizeFollowsColorTargetZero)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    Framebuffer framebuffer = context.makeFramebuffer();
    AttachmentFramebuffer const& attachments = *attachmentsOf(framebuffer);
    Vector2i const otherSize(4, 2);

    framebuffer.setColorTarget(1, context.makeTexture(otherSize, FORMAT_SRGBA));
    EXPECT_EQ(Vector2i(0, 0), attachments.snapshot().attachments.size);

    framebuffer.setColorTarget(0, context.makeTexture(size, FORMAT_SRGBA));
    AttachmentFramebuffer::Snapshot snapshot = attachments.snapshot();
    EXPECT_EQ(size, snapshot.attachments.size);
    EXPECT_EQ(attachments.getVersion(), snapshot.version);
    EXPECT_TRUE(snapshot.attachments.color[0].has_value());
    EXPECT_TRUE(snapshot.attachments.color[1].has_value());
    EXPECT_FALSE(snapshot.attachments.depth.has_value());

    framebuffer.setColorTarget(0,
        context.makeTexture(otherSize, FORMAT_SRGBA));
    EXPECT_EQ(otherSize, attachments.snapshot().attachments.size);

    framebuffer.unsetColorTarget(0);
    EXPECT_EQ(Vector2i(0, 0), attachments.snapshot().attachments.size);
}

TEST(framebuffer, windowFramebufferDescribesNoAttachments)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    Window window = platform.makeWindow(context, size, false);
    EXPECT_EQ(nullptr, attachmentsOf(window.getDefaultFramebuffer()));
}
