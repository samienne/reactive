#include <ase/blendmode.h>
#include <ase/buffer.h>
#include <ase/dummyplatform.h>
#include <ase/namedvertexspec.h>
#include <ase/pipeline.h>
#include <ase/pipelinedesc.h>
#include <ase/platform.h>
#include <ase/program.h>
#include <ase/rendercontext.h>
#include <ase/shader.h>
#include <ase/shaderinterface.h>
#include <ase/shaderlanguage.h>
#include <ase/vertexspec.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace ase;

namespace
{
    Buffer bytes(std::string const& source)
    {
        return Buffer(source.data(), source.size());
    }

    ShaderInterface positionInterface()
    {
        ShaderInterface iface;
        iface.attribute("pos", 0, TypeFloat, 3);
        return iface;
    }

    PipelineDesc describe(std::optional<BlendFunc> blend,
            bool depthTest = true,
            std::optional<bool> depthWrite = std::nullopt)
    {
        PipelineDesc desc;
        desc.blend = blend;
        desc.depthTest = depthTest;
        desc.depthWrite = depthWrite;
        return desc;
    }

    struct Fixture
    {
        btl::RunLoop loop;
        Platform platform = makeDummyPlatform(loop);
        RenderContext context = platform.makeRenderContext();
        Program program = makeProgram();
        VertexSpec spec = vertexSpec();
        BlendFunc const over{ BlendMode::One, BlendMode::OneMinusSrcAlpha };
        PipelineDesc const blendedDesc = describe(over);

        Program makeProgram()
        {
            Shader vs = context.makeShader(ShaderStage::Vertex,
                    ShaderLanguage::Glsl330, bytes("vertex"));
            Shader fs = context.makeShader(ShaderStage::Fragment,
                    ShaderLanguage::Glsl330, bytes("fragment"));
            return context.makeProgram(vs, fs, positionInterface());
        }

        VertexSpec vertexSpec() const
        {
            NamedVertexSpec named;
            named.add("pos", 3, TypeFloat, false);
            return VertexSpec(program, named);
        }
    };
} // anonymous namespace

TEST(pipeline, defaultIsOpaqueAndTestsAndWritesDepth)
{
    Fixture f;
    Pipeline pipeline = f.context.makePipeline(f.program, f.spec);

    EXPECT_FALSE(pipeline.isBlendEnabled());
    EXPECT_TRUE(pipeline.isDepthTestEnabled());
    EXPECT_TRUE(pipeline.isDepthWriteEnabled());
}

TEST(pipeline, blendedLeavesDepthAloneByDefault)
{
    Fixture f;
    Pipeline withBlend = f.context.makePipelineWithBlend(f.program, f.spec,
            BlendMode::One, BlendMode::OneMinusSrcAlpha);
    Pipeline described = f.context.makePipeline(f.program, f.spec,
            f.blendedDesc);

    for (Pipeline const& pipeline : { withBlend, described })
    {
        EXPECT_TRUE(pipeline.isBlendEnabled());
        EXPECT_TRUE(pipeline.isDepthTestEnabled());
        EXPECT_FALSE(pipeline.isDepthWriteEnabled());
    }
}

TEST(pipeline, depthFlagsAreExplicit)
{
    Fixture f;
    Pipeline effect = f.context.makePipeline(f.program, f.spec,
            describe(std::nullopt, false));
    EXPECT_FALSE(effect.isBlendEnabled());
    EXPECT_FALSE(effect.isDepthTestEnabled());
    EXPECT_TRUE(effect.isDepthWriteEnabled());

    Pipeline blended = f.context.makePipeline(f.program, f.spec,
            describe(f.over, true, true));
    EXPECT_TRUE(blended.isBlendEnabled());
    EXPECT_TRUE(blended.isDepthTestEnabled());
    EXPECT_TRUE(blended.isDepthWriteEnabled());

    Pipeline opaque = f.context.makePipeline(f.program, f.spec,
            describe(std::nullopt, true, false));
    EXPECT_FALSE(opaque.isBlendEnabled());
    EXPECT_TRUE(opaque.isDepthTestEnabled());
    EXPECT_FALSE(opaque.isDepthWriteEnabled());
}

TEST(pipelineDesc, resolvesDepthWriteFromBlend)
{
    BlendFunc const add{ BlendMode::One, BlendMode::One };
    EXPECT_TRUE(PipelineDesc{}.writesDepth());
    EXPECT_FALSE(describe(add).writesDepth());
    EXPECT_TRUE(describe(add, true, true).writesDepth());
    EXPECT_FALSE(describe(std::nullopt, true, false).writesDepth());
}
