#include <ase/buffer.h>
#include <ase/commandbuffer.h>
#include <ase/drawcommand.h>
#include <ase/dummyplatform.h>
#include <ase/format.h>
#include <ase/framebuffer.h>
#include <ase/namedvertexspec.h>
#include <ase/pipeline.h>
#include <ase/pipelinedesc.h>
#include <ase/platform.h>
#include <ase/program.h>
#include <ase/rendercommand.h>
#include <ase/rendercontext.h>
#include <ase/scissorrect.h>
#include <ase/shader.h>
#include <ase/shaderinterface.h>
#include <ase/shaderlanguage.h>
#include <ase/uniformset.h>
#include <ase/usage.h>
#include <ase/vertexbuffer.h>
#include <ase/vertexspec.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

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

    struct Fixture
    {
        btl::RunLoop loop;
        Platform platform = makeDummyPlatform(loop);
        RenderContext context = platform.makeRenderContext();
        Program program = makeProgram();
        Pipeline pipeline = context.makePipeline(program, vertexSpec(),
                PipelineDesc{ FORMAT_SRGBA });
        Framebuffer framebuffer = makeFramebuffer();
        UniformSet uniforms = context.makeUniformSet(positionInterface());

        Framebuffer makeFramebuffer()
        {
            Framebuffer framebuffer = context.makeFramebuffer();
            framebuffer.setColorTarget(0, context.makeTexture(Vector2i(4, 4),
                        FORMAT_SRGBA));
            return framebuffer;
        }

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

        void pushDraw(CommandBuffer& commands, float z,
                std::optional<ScissorRect> scissor = std::nullopt)
        {
            commands.push(framebuffer, pipeline, uniforms,
                    context.makeVertexBuffer(), std::nullopt, {}, z, scissor);
        }

        void pushDrawWithoutDepthTest(CommandBuffer& commands, float z)
        {
            PipelineDesc desc{ FORMAT_SRGBA };
            desc.depthTest = false;
            commands.push(framebuffer,
                    context.makePipeline(program, vertexSpec(), desc),
                    uniforms, context.makeVertexBuffer(), std::nullopt, {}, z,
                    std::nullopt);
        }
    };

    std::vector<float> drawOrder(CommandBuffer const& commands)
    {
        std::vector<float> zs;
        for (auto const& command : commands)
        {
            if (auto const* draw = std::get_if<DrawCommand>(&command))
                zs.push_back(draw->getZ());
        }

        return zs;
    }

    bool byAscendingZ(RenderCommand const& a, RenderCommand const& b)
    {
        return std::get<DrawCommand>(a).getZ() < std::get<DrawCommand>(b).getZ();
    }
} // anonymous namespace

TEST(commandBuffer, barrierSplitsTheDrawRuns)
{
    Fixture f;
    CommandBuffer commands;
    f.pushDraw(commands, 4.0f);
    f.pushDraw(commands, 3.0f);
    commands.pushBarrier();
    f.pushDraw(commands, 2.0f);
    f.pushDraw(commands, 1.0f);

    ASSERT_EQ(5u, commands.size());
    EXPECT_TRUE(std::holds_alternative<BarrierCommand>(
                *(commands.begin() + 2)));

    sortDrawRuns(commands.begin(), commands.end(), byAscendingZ);

    EXPECT_EQ((std::vector<float>{ 3.0f, 4.0f, 1.0f, 2.0f }),
            drawOrder(commands));
    EXPECT_TRUE(std::holds_alternative<BarrierCommand>(
                *(commands.begin() + 2)));
}

TEST(commandBuffer, barriersAtTheEndsLeaveOneRun)
{
    Fixture f;
    CommandBuffer commands;
    commands.pushBarrier();
    f.pushDraw(commands, 3.0f);
    f.pushDraw(commands, 2.0f);
    f.pushDraw(commands, 1.0f);
    commands.pushBarrier();

    sortDrawRuns(commands.begin(), commands.end(), byAscendingZ);

    EXPECT_EQ((std::vector<float>{ 1.0f, 2.0f, 3.0f }), drawOrder(commands));
    EXPECT_TRUE(std::holds_alternative<BarrierCommand>(*commands.begin()));
    EXPECT_TRUE(std::holds_alternative<BarrierCommand>(
                *(commands.begin() + 4)));
}

TEST(commandBuffer, consecutiveBarriersSplitOnce)
{
    Fixture f;
    CommandBuffer commands;
    f.pushDraw(commands, 2.0f);
    f.pushDraw(commands, 1.0f);
    commands.pushBarrier();
    commands.pushBarrier();
    f.pushDraw(commands, 4.0f);
    f.pushDraw(commands, 3.0f);

    sortDrawRuns(commands.begin(), commands.end(), byAscendingZ);

    EXPECT_EQ((std::vector<float>{ 1.0f, 2.0f, 3.0f, 4.0f }),
            drawOrder(commands));
    EXPECT_TRUE(std::holds_alternative<BarrierCommand>(
                *(commands.begin() + 2)));
    EXPECT_TRUE(std::holds_alternative<BarrierCommand>(
                *(commands.begin() + 3)));
}

TEST(commandBuffer, clearAndUploadKeepTheirPlace)
{
    Fixture f;
    CommandBuffer commands;
    f.pushDraw(commands, 2.0f);
    f.pushDraw(commands, 1.0f);
    commands.pushClear(f.framebuffer);
    f.pushDraw(commands, 4.0f);
    f.pushDraw(commands, 3.0f);
    commands.pushUpload(f.context.makeVertexBuffer(), bytes("data"),
            Usage::StaticDraw);
    f.pushDraw(commands, 6.0f);
    f.pushDraw(commands, 5.0f);

    sortDrawRuns(commands.begin(), commands.end(), byAscendingZ);

    EXPECT_EQ((std::vector<float>{ 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f }),
            drawOrder(commands));
    EXPECT_TRUE(std::holds_alternative<ClearCommand>(*(commands.begin() + 2)));
    EXPECT_TRUE(std::holds_alternative<BufferUploadCommand>(
                *(commands.begin() + 5)));
}

TEST(commandBuffer, drawWithoutDepthTestKeepsItsPlace)
{
    Fixture f;
    CommandBuffer commands;
    f.pushDraw(commands, 3.0f);
    f.pushDraw(commands, 2.0f);
    f.pushDrawWithoutDepthTest(commands, 9.0f);
    f.pushDraw(commands, 1.0f);
    f.pushDraw(commands, 0.0f);

    sortDrawRuns(commands.begin(), commands.end(), byAscendingZ);

    EXPECT_EQ((std::vector<float>{ 2.0f, 3.0f, 9.0f, 0.0f, 1.0f }),
            drawOrder(commands));
}

TEST(commandBuffer, drawsWithoutABarrierFormOneRun)
{
    Fixture f;
    CommandBuffer commands;
    f.pushDraw(commands, 4.0f);
    f.pushDraw(commands, 3.0f);
    f.pushDraw(commands, 2.0f);
    f.pushDraw(commands, 1.0f);

    sortDrawRuns(commands.begin(), commands.end(), byAscendingZ);

    EXPECT_EQ((std::vector<float>{ 1.0f, 2.0f, 3.0f, 4.0f }),
            drawOrder(commands));
}

TEST(commandBuffer, drawsAreUnscissoredByDefault)
{
    Fixture f;
    CommandBuffer commands;
    f.pushDraw(commands, 1.0f);
    commands.pushClear(f.framebuffer);

    EXPECT_FALSE(std::get<DrawCommand>(*commands.begin()).getScissor());
    EXPECT_FALSE(std::get<ClearCommand>(*(commands.begin() + 1)).scissor);
}

TEST(commandBuffer, drawAndClearCarryTheirScissor)
{
    Fixture f;
    CommandBuffer commands;
    ScissorRect const rect{ 8, 16, 32, 24 };
    f.pushDraw(commands, 1.0f, rect);
    commands.pushClear(f.framebuffer, 0.0f, 0.0f, 0.0f, 0.0f, true, false,
            false, rect);

    auto const& draw = std::get<DrawCommand>(*commands.begin());
    auto const& clear = std::get<ClearCommand>(*(commands.begin() + 1));
    ASSERT_TRUE(draw.getScissor());
    EXPECT_EQ(rect, *draw.getScissor());
    ASSERT_TRUE(clear.scissor);
    EXPECT_EQ(rect, *clear.scissor);
    EXPECT_FALSE(clear.depth);
}

TEST(scissorRect, ordersByEachField)
{
    ScissorRect const a{ 0, 0, 10, 10 };
    EXPECT_EQ(a, (ScissorRect{ 0, 0, 10, 10 }));
    EXPECT_NE(a, (ScissorRect{ 0, 0, 10, 11 }));
    EXPECT_LT(a, (ScissorRect{ 0, 0, 10, 11 }));
    EXPECT_LT(a, (ScissorRect{ 1, 0, 0, 0 }));
    EXPECT_FALSE(a < a);
}
