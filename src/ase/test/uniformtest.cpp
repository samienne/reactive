#include <ase/buffer.h>
#include <ase/commandbuffer.h>
#include <ase/dummyplatform.h>
#include <ase/dummyuniformset.h>
#include <ase/matrix.h>
#include <ase/platform.h>
#include <ase/rendercommand.h>
#include <ase/rendercontext.h>
#include <ase/shaderinterface.h>
#include <ase/uniformblock.h>
#include <ase/uniformbufferimpl.h>
#include <ase/uniformring.h>
#include <ase/uniformset.h>
#include <ase/uniformvalues.h>
#include <ase/vector.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>
#include <vector>

using namespace ase;

namespace
{
    // std140: radius at 0, direction at 8, transform at 16, weights at 80
    // with a 16-byte stride, 112 bytes in all.
    ShaderInterface effectInterface()
    {
        ShaderInterface iface;
        iface.block("BlurParams", 2, BlockLayout::Std140, {
                { "radius", UniformType::Float },
                { "direction", UniformType::Float2 },
                { "transform", UniformType::Mat4 },
                { "weights", UniformType::Float, 2 },
            });
        return iface;
    }

    struct BlurParams
    {
        float radius;
        float pad0;
        Vector2f direction;
        Matrix4f transform;
        float weights[2][4];
    };

    std::vector<UniformBlockMember<BlurParams>> blurMembers()
    {
        return {
            blockMember<&BlurParams::radius>("radius"),
            blockMember<&BlurParams::direction>("direction"),
            blockMember<&BlurParams::transform>("transform"),
            blockMember<&BlurParams::weights>("weights"),
        };
    }

    ShaderInterface matrixInterface()
    {
        ShaderInterface iface;
        iface.block("MatrixBlock", 0, BlockLayout::Std140, {
                { "worldViewProj", UniformType::Mat4 },
            });
        return iface;
    }

    struct MatrixBlock
    {
        Matrix4f worldViewProj;
    };

    float floatAt(void const* data, size_t offset)
    {
        float value;
        std::memcpy(&value, static_cast<char const*>(data) + offset,
                sizeof(value));
        return value;
    }

    struct Fixture
    {
        btl::RunLoop loop;
        Platform platform = makeDummyPlatform(loop);
        RenderContext context = platform.makeRenderContext();
    };

    UniformBufferImpl const* implOf(UniformBuffer const& buffer)
    {
        return &buffer.getImpl<UniformBufferImpl>();
    }
} // anonymous namespace

TEST(uniformBlock, matchingStructIsAccepted)
{
    UniformBlock<BlurParams> params(effectInterface(), "BlurParams",
            blurMembers());

    EXPECT_EQ(sizeof(BlurParams), params.getSize());
    EXPECT_EQ(112u, params.getBlock().size);

    params->radius = 3.0f;
    params->weights[1][0] = 7.0f;

    EXPECT_EQ(3.0f, floatAt(params.data(), 0));
    EXPECT_EQ(7.0f, floatAt(params.data(), 96));
}

TEST(uniformBlock, wrongSizeIsRejected)
{
    struct Short
    {
        Matrix3f worldViewProj;
    };

    std::vector<UniformBlockMember<Short>> members = {
        blockMember<&Short::worldViewProj>("worldViewProj"),
    };

    EXPECT_THROW(UniformBlock<Short>(matrixInterface(), "MatrixBlock", members),
            std::invalid_argument);
}

TEST(uniformBlock, misPaddedStructIsRejected)
{
    struct Unpadded
    {
        float radius;
        Vector2f direction;
        float pad0;
        Matrix4f transform;
        float weights[2][4];
    };
    static_assert(sizeof(Unpadded) == sizeof(BlurParams));

    std::vector<UniformBlockMember<Unpadded>> members = {
        blockMember<&Unpadded::radius>("radius"),
        blockMember<&Unpadded::direction>("direction"),
        blockMember<&Unpadded::transform>("transform"),
        blockMember<&Unpadded::weights>("weights"),
    };

    EXPECT_THROW(UniformBlock<Unpadded>(effectInterface(), "BlurParams",
                members), std::invalid_argument);
}

TEST(uniformBlock, wrongMemberSizeIsRejected)
{
    struct Tight
    {
        float radius;
        float pad0;
        Vector2f direction;
        Matrix4f transform;
        float weights[2];
        float pad1[6];
    };
    static_assert(sizeof(Tight) == sizeof(BlurParams));

    std::vector<UniformBlockMember<Tight>> members = {
        blockMember<&Tight::radius>("radius"),
        blockMember<&Tight::direction>("direction"),
        blockMember<&Tight::transform>("transform"),
        blockMember<&Tight::weights>("weights"),
    };

    EXPECT_THROW(UniformBlock<Tight>(effectInterface(), "BlurParams", members),
            std::invalid_argument);
}

TEST(uniformBlock, everyDeclaredMemberMustBeListed)
{
    std::vector<UniformBlockMember<BlurParams>> incomplete = {
        blockMember<&BlurParams::radius>("radius"),
    };
    std::vector<UniformBlockMember<BlurParams>> misnamed = blurMembers();
    misnamed.back().name = "sigma";

    EXPECT_THROW(UniformBlock<BlurParams>(effectInterface(), "BlurParams",
                incomplete), std::invalid_argument);
    EXPECT_THROW(UniformBlock<BlurParams>(effectInterface(), "BlurParams",
                misnamed), std::invalid_argument);

    EXPECT_THROW(UniformBlock<BlurParams>(effectInterface(), "NoSuchBlock",
                blurMembers()), std::invalid_argument);
}

TEST(uniformValues, writesAtDeclaredOffsets)
{
    UniformValues values(effectInterface(), "BlurParams");
    ASSERT_EQ(112u, values.getSize());

    Matrix4f transform = Matrix4f::Identity();
    transform(1, 0) = 5.0f;

    values.set("radius", 12.0f);
    values.set("direction", Vector2f(1.0f, 0.5f));
    values.set("transform", transform);
    values.set("weights", std::vector<float>{ 0.25f, 0.75f });

    EXPECT_EQ(12.0f, floatAt(values.data(), 0));
    EXPECT_EQ(1.0f, floatAt(values.data(), 8));
    EXPECT_EQ(0.5f, floatAt(values.data(), 12));
    EXPECT_EQ(1.0f, floatAt(values.data(), 16));
    EXPECT_EQ(5.0f, floatAt(values.data(), 20));
    EXPECT_EQ(0.25f, floatAt(values.data(), 80));
    EXPECT_EQ(0.75f, floatAt(values.data(), 96));
}

TEST(uniformValues, agreesWithTypedBlock)
{
    UniformBlock<BlurParams> block(effectInterface(), "BlurParams",
            blurMembers());
    std::memset(&*block, 0, sizeof(BlurParams));
    block->radius = 12.0f;
    block->direction = Vector2f(1.0f, 0.5f);
    block->transform = Matrix4f::Identity();
    block->weights[0][0] = 0.25f;
    block->weights[1][0] = 0.75f;

    Matrix4f const identity = Matrix4f::Identity();
    UniformValues values(effectInterface(), "BlurParams");
    values.set("radius", 12.0f);
    values.set("direction", Vector2f(1.0f, 0.5f));
    values.set("transform", identity);
    values.set("weights", std::vector<float>{ 0.25f, 0.75f });

    ASSERT_EQ(block.getSize(), values.getSize());
    EXPECT_EQ(0, std::memcmp(block.data(), values.data(), block.getSize()));
}

TEST(uniformValues, padsMatrixColumns)
{
    ShaderInterface iface;
    iface.block("B", 0, BlockLayout::Std140, {
            { "m", UniformType::Mat3 },
        });

    Matrix3f m;
    m << 1, 2, 3,
         4, 5, 6,
         7, 8, 9;

    UniformValues values(iface, "B");
    values.set("m", m);

    ASSERT_EQ(48u, values.getSize());
    EXPECT_EQ(1.0f, floatAt(values.data(), 0));
    EXPECT_EQ(4.0f, floatAt(values.data(), 4));
    EXPECT_EQ(7.0f, floatAt(values.data(), 8));
    EXPECT_EQ(2.0f, floatAt(values.data(), 16));
    EXPECT_EQ(3.0f, floatAt(values.data(), 32));
    EXPECT_EQ(9.0f, floatAt(values.data(), 40));
}

TEST(uniformValues, rejectsUnknownNamesAndWrongTypes)
{
    UniformValues values(effectInterface(), "BlurParams");

    EXPECT_THROW(values.set("sigma", 1.0f), std::invalid_argument);
    EXPECT_THROW(values.set("radius", 1), std::invalid_argument);
    EXPECT_THROW(values.set("direction", Vector3f(1.0f, 0.0f, 0.0f)),
            std::invalid_argument);
    EXPECT_THROW(values.set("radius", std::vector<float>{ 1.0f }),
            std::invalid_argument);
    EXPECT_THROW(values.set("weights", 1.0f), std::invalid_argument);
    EXPECT_THROW(values.set("weights", std::vector<float>{ 1.0f }),
            std::invalid_argument);
    EXPECT_THROW(UniformValues(effectInterface(), "NoSuchBlock"),
            std::invalid_argument);
}

TEST(uniformRing, alignsRangesAndWrapsToTheNextBuffer)
{
    Fixture f;
    UniformRing ring(f.context, 2);
    ASSERT_EQ(256u, ring.getAlignment());
    EXPECT_EQ(2, ring.getFramesInFlight());

    float const a = 1.0f;
    float const b = 2.0f;

    ring.beginFrame();
    UniformBufferRange first = ring.write(&a, sizeof(a));
    UniformBufferRange second = ring.write(&b, sizeof(b));
    EXPECT_EQ(0u, first.offset);
    EXPECT_EQ(sizeof(a), first.size);
    EXPECT_EQ(256u, second.offset);
    EXPECT_EQ(implOf(first.buffer), implOf(second.buffer));

    ring.beginFrame();
    UniformBufferRange third = ring.write(&a, sizeof(a));
    EXPECT_EQ(0u, third.offset);
    EXPECT_NE(implOf(first.buffer), implOf(third.buffer));

    ring.beginFrame();
    UniformBufferRange fourth = ring.write(&a, sizeof(a));
    EXPECT_EQ(0u, fourth.offset);
    EXPECT_EQ(implOf(first.buffer), implOf(fourth.buffer));
}

TEST(uniformRing, uploadsTheWrittenSpanOncePerFrame)
{
    Fixture f;
    UniformRing ring(f.context);

    UniformBlock<MatrixBlock> matrices(matrixInterface(), "MatrixBlock", {
            blockMember<&MatrixBlock::worldViewProj>("worldViewProj"),
        });
    matrices->worldViewProj = Matrix4f::Identity() * 3.0f;

    float const tail = 9.0f;

    ring.beginFrame();
    UniformBufferRange first = ring.write(matrices);
    UniformBufferRange second = ring.write(&tail, sizeof(tail));

    CommandBuffer uploads;
    ring.endFrame(uploads);

    ASSERT_EQ(1u, uploads.size());
    auto const* upload = std::get_if<BufferUploadCommand>(&*uploads.begin());
    ASSERT_TRUE(upload);
    ASSERT_TRUE(std::holds_alternative<UniformBuffer>(upload->target));
    EXPECT_EQ(implOf(first.buffer),
            implOf(std::get<UniformBuffer>(upload->target)));
    EXPECT_EQ(second.offset + sizeof(tail), upload->data.getSize());
    EXPECT_EQ(3.0f, floatAt(upload->data.data(), first.offset));
    EXPECT_EQ(3.0f, floatAt(upload->data.data(), first.offset + 60));
    EXPECT_EQ(9.0f, floatAt(upload->data.data(), second.offset));

    EXPECT_THROW(ring.write(&tail, sizeof(tail)), std::logic_error);

    ring.beginFrame();
    CommandBuffer empty;
    ring.endFrame(empty);
    EXPECT_EQ(0u, empty.size());
}

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
