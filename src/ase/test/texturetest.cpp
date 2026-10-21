#include <ase/buffer.h>
#include <ase/commandbuffer.h>
#include <ase/dummyplatform.h>
#include <ase/format.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/renderqueue.h>
#include <ase/texture.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace ase;

TEST(texture, r8IsOneLinearByte)
{
    EXPECT_EQ(1u, getBytes(FORMAT_R8));
    EXPECT_TRUE(isLinear(FORMAT_R8));
}

TEST(texture, subUploadKeepsSizeAndFormat)
{
    btl::RunLoop loop;
    Platform platform = makeDummyPlatform(loop);
    RenderContext context = platform.makeRenderContext();

    Texture texture = context.makeTexture(Vector2i(8, 4), FORMAT_R8);

    std::vector<uint8_t> pixels(2 * 3, 255);
    CommandBuffer commands;
    commands.pushSubUpload(texture, Buffer(pixels), Vector2i(5, 1),
            Vector2i(2, 3));
    EXPECT_EQ(1u, commands.size());

    context.getMainRenderQueue().submit(std::move(commands));

    EXPECT_EQ(Vector2i(8, 4), texture.getSize());
    EXPECT_EQ(FORMAT_R8, texture.getFormat());
}
