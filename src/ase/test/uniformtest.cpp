#include <ase/dummyplatform.h>
#include <ase/dummyuniformset.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/shaderinterface.h>
#include <ase/uniformset.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <stdexcept>

using namespace ase;

namespace
{
    ShaderInterface matrixInterface()
    {
        ShaderInterface iface;
        iface.block("MatrixBlock", 0, BlockLayout::Std140, {
                { "worldViewProj", UniformType::Mat4 },
            });
        return iface;
    }

    struct Fixture
    {
        btl::RunLoop loop;
        Platform platform = makeDummyPlatform(loop);
        RenderContext context = platform.makeRenderContext();
    };
} // anonymous namespace

TEST(uniformSet, rebindOverwrites)
{
    Fixture f;
    UniformSet set = f.context.makeUniformSet(matrixInterface());
    UniformBuffer buffer = f.context.makeUniformBuffer();

    set.bindUniformBufferRange(0, UniformBufferRange{ 0, 64, buffer });
    set.bindUniformBufferRange("MatrixBlock",
            UniformBufferRange{ 256, 64, buffer });

    auto const& bindings = set.getImpl<DummyUniformSet>().getBindings();
    ASSERT_EQ(1u, bindings.size());
    EXPECT_EQ(256u, bindings.at(0).offset);
}

TEST(uniformSet, rejectsUndeclaredBlocksAndShortRanges)
{
    Fixture f;
    UniformSet set = f.context.makeUniformSet(matrixInterface());
    UniformBuffer buffer = f.context.makeUniformBuffer();

    EXPECT_THROW(set.bindUniformBufferRange(1,
                UniformBufferRange{ 0, 64, buffer }), std::invalid_argument);
    EXPECT_THROW(set.bindUniformBufferRange("BlurParams",
                UniformBufferRange{ 0, 64, buffer }), std::invalid_argument);
    EXPECT_THROW(set.bindUniformBufferRange(0,
                UniformBufferRange{ 0, 48, buffer }), std::invalid_argument);
    EXPECT_TRUE(set.getImpl<DummyUniformSet>().getBindings().empty());
}
