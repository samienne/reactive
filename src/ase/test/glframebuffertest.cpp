#include "gl/glfunctions.h"

#include <ase/commandbuffer.h>
#include <ase/format.h>
#include <ase/framebuffer.h>
#include <ase/glrendercontext.h>
#include <ase/glrenderqueue.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/texture.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace ase;

namespace
{
    Vector2i const size(16, 8);

    class GlFramebufferTest : public testing::Test
    {
    protected:
        GlRenderQueue& mainQueue()
        {
            return context.getImpl<GlRenderContext>().getMainGlRenderQueue();
        }

        GlRenderQueue& transferQueue()
        {
            return transferRenderQueue.getImpl<GlRenderQueue>();
        }

        void clear(RenderQueue queue, Framebuffer const& framebuffer)
        {
            CommandBuffer commands;
            commands.pushClear(framebuffer);
            queue.submit(std::move(commands));
            queue.flush();
        }

        void submitNothing(RenderQueue queue)
        {
            queue.submit(CommandBuffer());
            queue.flush();
        }

        Framebuffer makeColorFramebuffer()
        {
            Framebuffer framebuffer = context.makeFramebuffer();
            framebuffer.setColorTarget(0,
                    context.makeTexture(size, FORMAT_SRGBA));
            return framebuffer;
        }

        static size_t countFramebufferObjects(GlRenderQueue& queue)
        {
            size_t count = 0;
            queue.dispatch([&count](GlFunctions const& gl)
                {
                    for (GLuint name = 1; name <= 4096; ++name)
                    {
                        if (gl.glIsFramebuffer(name))
                            ++count;
                    }
                });
            queue.flush();
            return count;
        }

        static GLint boundAttachment(GlRenderQueue& queue, GLenum attachment,
                GLenum parameter)
        {
            GLint value = -1;
            queue.dispatch([&](GlFunctions const& gl)
                {
                    gl.glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                            attachment, parameter, &value);
                });
            queue.flush();
            return value;
        }

        btl::RunLoop loop;
        Platform platform = makeDefaultPlatform(loop);
        RenderContext context = platform.makeRenderContext();
        RenderQueue mainRenderQueue = context.getMainRenderQueue();
        RenderQueue transferRenderQueue = context.getTransferQueue();
    };
} // anonymous namespace

TEST_F(GlFramebufferTest, droppedFramebuffersLeaveNoObjects)
{
    size_t const mainBefore = countFramebufferObjects(mainQueue());
    size_t const transferBefore = countFramebufferObjects(transferQueue());

    for (int round = 0; round < 100; ++round)
    {
        Framebuffer framebuffer = makeColorFramebuffer();
        clear(mainRenderQueue, framebuffer);
        clear(transferRenderQueue, framebuffer);
    }

    submitNothing(mainRenderQueue);
    submitNothing(transferRenderQueue);

    EXPECT_EQ(mainBefore, countFramebufferObjects(mainQueue()));
    EXPECT_EQ(transferBefore, countFramebufferObjects(transferQueue()));
}

TEST_F(GlFramebufferTest, idleQueueSweepsDroppedFramebuffers)
{
    size_t const before = countFramebufferObjects(mainQueue());

    std::vector<Framebuffer> framebuffers;
    for (int i = 0; i < 4; ++i)
    {
        framebuffers.push_back(makeColorFramebuffer());
        clear(mainRenderQueue, framebuffers.back());
    }

    EXPECT_EQ(before + 4, countFramebufferObjects(mainQueue()));
    framebuffers.clear();

    // The sweep runs after a second without work, and every probe is work.
    size_t after = 0;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        after = countFramebufferObjects(mainQueue());
        if (after == before)
            break;
    }

    EXPECT_EQ(before, after);
}

TEST_F(GlFramebufferTest, unsetColorTargetIsDetached)
{
    Framebuffer framebuffer = makeColorFramebuffer();
    framebuffer.setColorTarget(1, context.makeTexture(size, FORMAT_SRGBA));
    clear(mainRenderQueue, framebuffer);

    EXPECT_EQ(GL_TEXTURE, boundAttachment(mainQueue(), GL_COLOR_ATTACHMENT1,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));

    framebuffer.unsetColorTarget(1);
    clear(mainRenderQueue, framebuffer);

    EXPECT_EQ(GL_NONE, boundAttachment(mainQueue(), GL_COLOR_ATTACHMENT1,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
    EXPECT_EQ(GL_TEXTURE, boundAttachment(mainQueue(), GL_COLOR_ATTACHMENT0,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
}

TEST_F(GlFramebufferTest, changedTargetIsAttachedOnNextUse)
{
    Framebuffer framebuffer = makeColorFramebuffer();
    clear(mainRenderQueue, framebuffer);
    GLint const first = boundAttachment(mainQueue(), GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);

    framebuffer.setColorTarget(0, context.makeTexture(size, FORMAT_SRGBA));
    size_t const objects = countFramebufferObjects(mainQueue());
    clear(mainRenderQueue, framebuffer);
    GLint const second = boundAttachment(mainQueue(), GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);

    EXPECT_NE(0, first);
    EXPECT_NE(0, second);
    EXPECT_NE(first, second);
    EXPECT_EQ(objects, countFramebufferObjects(mainQueue()));
}
