#include <ase/blendmode.h>
#include <ase/buffer.h>
#include <ase/dummyplatform.h>
#include <ase/format.h>
#include <ase/framebuffer.h>
#include <ase/namedvertexspec.h>
#include <ase/pipeline.h>
#include <ase/pipelinedesc.h>
#include <ase/platform.h>
#include <ase/program.h>
#include <ase/renderbuffer.h>
#include <ase/rendercontext.h>
#include <ase/shader.h>
#include <ase/shaderinterface.h>
#include <ase/shaderlanguage.h>
#include <ase/texture.h>
#include <ase/vertexspec.h>
#include <ase/window.h>

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
        PipelineDesc desc{ FORMAT_SRGBA, FORMAT_DEPTH24_STENCIL8 };
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
        Vector2i const size{ 16, 8 };

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

        Pipeline pipeline(Format color, std::optional<Format> depth)
        {
            return context.makePipeline(program, spec,
                    PipelineDesc{ color, depth });
        }

        Framebuffer target(Format color, std::optional<Format> depth)
        {
            Framebuffer framebuffer = context.makeFramebuffer();
            framebuffer.setColorTarget(0, context.makeTexture(size, color));
            if (depth)
            {
                framebuffer.setDepthTarget(
                        context.makeRenderbuffer(size, *depth));
            }
            return framebuffer;
        }
    };
} // anonymous namespace

TEST(pipeline, opaqueTestsAndWritesDepth)
{
    Fixture f;
    Pipeline pipeline = f.context.makePipeline(f.program, f.spec,
            describe(std::nullopt));

    EXPECT_FALSE(pipeline.isBlendEnabled());
    EXPECT_TRUE(pipeline.isDepthTestEnabled());
    EXPECT_TRUE(pipeline.isDepthWriteEnabled());
}

TEST(pipeline, blendedLeavesDepthAloneByDefault)
{
    Fixture f;
    Pipeline pipeline = f.context.makePipeline(f.program, f.spec,
            f.blendedDesc);

    EXPECT_TRUE(pipeline.isBlendEnabled());
    EXPECT_TRUE(pipeline.isDepthTestEnabled());
    EXPECT_FALSE(pipeline.isDepthWriteEnabled());
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

TEST(pipeline, recordsItsTargetFormats)
{
    Fixture f;

    Pipeline withDepth = f.pipeline(FORMAT_SRGBA, FORMAT_DEPTH24_STENCIL8);
    EXPECT_EQ(FORMAT_SRGBA, withDepth.getColorFormat());
    EXPECT_EQ(FORMAT_DEPTH24_STENCIL8, withDepth.getDepthFormat());

    Pipeline colorOnly = f.pipeline(FORMAT_RGBA16F, std::nullopt);
    EXPECT_EQ(FORMAT_RGBA16F, colorOnly.getColorFormat());
    EXPECT_FALSE(colorOnly.getDepthFormat().has_value());
}

TEST(pipeline, matchesTargetsOfItsDeclaredFormats)
{
    Fixture f;

    Pipeline withDepth = f.pipeline(FORMAT_SRGBA, FORMAT_DEPTH24_STENCIL8);
    EXPECT_TRUE(withDepth.matchesTarget(
                f.target(FORMAT_SRGBA, FORMAT_DEPTH24_STENCIL8)));
    EXPECT_FALSE(withDepth.matchesTarget(
                f.target(FORMAT_RGBA16F, FORMAT_DEPTH24_STENCIL8)));
    EXPECT_FALSE(withDepth.matchesTarget(
                f.target(FORMAT_SRGBA, FORMAT_DEPTH24)));
    EXPECT_FALSE(withDepth.matchesTarget(f.target(FORMAT_SRGBA, std::nullopt)));

    Pipeline colorOnly = f.pipeline(FORMAT_RGBA16F, std::nullopt);
    EXPECT_TRUE(colorOnly.matchesTarget(
                f.target(FORMAT_RGBA16F, std::nullopt)));
    EXPECT_FALSE(colorOnly.matchesTarget(
                f.target(FORMAT_RGBA16F, FORMAT_DEPTH16)));
    EXPECT_FALSE(colorOnly.matchesTarget(f.target(FORMAT_R8, std::nullopt)));
}

TEST(pipeline, matchesTheWindowFramebufferItWasDeclaredFor)
{
    Fixture f;
    Window window = f.platform.makeWindow(f.context, f.size, true);
    Framebuffer const& target = window.getDefaultFramebuffer();

    EXPECT_TRUE(f.pipeline(target.getColorFormat(), target.getDepthFormat())
            .matchesTarget(target));
    EXPECT_FALSE(f.pipeline(target.getColorFormat(), std::nullopt)
            .matchesTarget(target));
}

TEST(pipelineDesc, resolvesDepthWriteFromBlend)
{
    BlendFunc const add{ BlendMode::One, BlendMode::One };
    EXPECT_TRUE(PipelineDesc{ FORMAT_SRGBA }.writesDepth());
    EXPECT_FALSE(describe(add).writesDepth());
    EXPECT_TRUE(describe(add, true, true).writesDepth());
    EXPECT_FALSE(describe(std::nullopt, true, false).writesDepth());
}
