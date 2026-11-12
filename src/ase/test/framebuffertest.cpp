#include <ase/dummyplatform.h>
#include <ase/format.h>
#include <ase/framebuffer.h>
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
    Format const colorFormats[] = {
        FORMAT_SRGBA, FORMAT_SRGB, FORMAT_R8, FORMAT_RGBA16F
    };

    Format const depthFormats[] = {
        FORMAT_DEPTH16, FORMAT_DEPTH24, FORMAT_DEPTH24_STENCIL8
    };

    Vector2i const size(16, 8);
} // anonymous namespace

TEST(framebuffer, colorFormatsAttachAlone)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    for (Format format : colorFormats)
    {
        Texture texture = context.makeTexture(size, format);
        EXPECT_EQ(format, texture.getFormat());
        EXPECT_EQ(size, texture.getSize());

        Framebuffer framebuffer = context.makeFramebuffer();
        EXPECT_EQ(FORMAT_UNKNOWN, framebuffer.getColorFormat());
        EXPECT_FALSE(framebuffer.getDepthFormat().has_value());

        framebuffer.setColorTarget(0, texture);
        EXPECT_EQ(format, framebuffer.getColorFormat());
        EXPECT_FALSE(framebuffer.getDepthFormat().has_value());

        framebuffer.unsetColorTarget(0);
        EXPECT_EQ(FORMAT_UNKNOWN, framebuffer.getColorFormat());
    }
}

TEST(framebuffer, colorFormatsAttachWithDepth)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    for (Format depthFormat : depthFormats)
    {
        Renderbuffer depth = context.makeRenderbuffer(size, depthFormat);
        EXPECT_EQ(depthFormat, depth.getFormat());
        EXPECT_EQ(size, depth.getSize());

        for (Format format : colorFormats)
        {
            Framebuffer framebuffer = context.makeFramebuffer();
            framebuffer.setColorTarget(0, context.makeTexture(size, format));
            framebuffer.setDepthTarget(depth);
            EXPECT_EQ(format, framebuffer.getColorFormat());
            EXPECT_EQ(depthFormat, framebuffer.getDepthFormat());

            framebuffer.unsetDepthTarget();
            EXPECT_FALSE(framebuffer.getDepthFormat().has_value());
            framebuffer.unsetColorTarget(0);
        }
    }
}

TEST(framebuffer, windowReportsItsPixelFormat)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    Window window = platform.makeWindow(context, size, false);
    Framebuffer const& framebuffer = window.getDefaultFramebuffer();
    EXPECT_EQ(FORMAT_SRGBA, framebuffer.getColorFormat());
    EXPECT_EQ(FORMAT_DEPTH24_STENCIL8, framebuffer.getDepthFormat());

    // A headless window draws into attachments it made itself.
    Window headless = platform.makeWindow(context, size, true);
    Framebuffer const& offscreen = headless.getDefaultFramebuffer();
    EXPECT_EQ(FORMAT_SRGBA, offscreen.getColorFormat());
    EXPECT_EQ(FORMAT_DEPTH16, offscreen.getDepthFormat());
}
