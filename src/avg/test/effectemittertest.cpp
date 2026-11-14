#include <avg/builtineffects.h>
#include <avg/effect.h>
#include <avg/effectemitter.h>
#include <avg/effectplan.h>
#include <avg/effectshader.h>
#include <avg/passpipelinecache.h>
#include <avg/scratchpool.h>
#include <avg/targetimage.h>
#include <avg/color.h>

#include <ase/buffer.h>
#include <ase/commandbuffer.h>
#include <ase/drawcommand.h>
#include <ase/dummyplatform.h>
#include <ase/dummyuniformset.h>
#include <ase/format.h>
#include <ase/platform.h>
#include <ase/rendercommand.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>
#include <ase/uniformbufferrange.h>
#include <ase/uniformring.h>
#include <ase/uniformset.h>
#include <ase/vector.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <stdexcept>
#include <variant>
#include <vector>

using avg::Divisor;
using avg::Effect;
using avg::EffectRegions;
using avg::Geometry;
using avg::PassBlock;
using avg::PixelRect;
using avg::ScratchKey;
using avg::ScratchPlan;
using avg::ScratchSlot;
using avg::ScratchTimeline;
using avg::Vector2f;
using avg::Vector2i;

namespace
{
    Vector2i const screenSize(400, 300);
    PixelRect const glassBounds{ 100, 80, 200, 40 };

    PixelRect rect(int x, int y, int width, int height)
    {
        return PixelRect{ x, y, width, height };
    }

    ScratchSlot slot(Divisor divisor, ase::Format format, size_t index)
    {
        return ScratchSlot{ ScratchKey{ divisor, format }, index };
    }

    avg::FrostedGlassOptions glassWithRadius(float radius)
    {
        avg::FrostedGlassOptions options;
        options.radius = radius;
        return options;
    }

    // The bounds as two triangles, in screen pixels
    std::vector<Vector2f> triangles(PixelRect const& r)
    {
        float const x0 = static_cast<float>(r.x);
        float const y0 = static_cast<float>(r.y);
        float const x1 = static_cast<float>(r.x + r.width);
        float const y1 = static_cast<float>(r.y + r.height);

        return {
            Vector2f(x0, y0), Vector2f(x1, y0), Vector2f(x1, y1),
            Vector2f(x0, y0), Vector2f(x1, y1), Vector2f(x0, y1),
        };
    }

    struct Fixture
    {
        btl::RunLoop loop;
        ase::Platform platform = ase::makeDummyPlatform(loop);
        ase::RenderContext context = platform.makeRenderContext();
        avg::TargetImage target{ context, screenSize, ase::FORMAT_SRGBA };
        ase::UniformRing ring{ context };
        avg::ScratchPool pool{ context };
        avg::PassPipelineCache cache{ context };
        ase::CommandBuffer commands;
        ase::CommandBuffer uploads;

        void beginFrame()
        {
            pool.beginFrame(screenSize);
            cache.beginFrame();
            ring.beginFrame();
        }

        void endFrame()
        {
            ring.endFrame(uploads);
            pool.endFrame();
        }

        avg::EffectEmitter emitter()
        {
            return avg::EffectEmitter(context, commands, ring, pool, cache,
                    target);
        }

        // Every command but the vertex uploads feeding the draws
        std::vector<ase::RenderCommand const*> structure() const
        {
            std::vector<ase::RenderCommand const*> result;
            for (auto const& command : commands)
            {
                if (!std::holds_alternative<ase::BufferUploadCommand>(command))
                    result.push_back(&command);
            }

            return result;
        }

        std::vector<ase::DrawCommand const*> draws() const
        {
            std::vector<ase::DrawCommand const*> result;
            for (auto const& command : commands)
            {
                if (auto const* draw = std::get_if<ase::DrawCommand>(&command))
                    result.push_back(draw);
            }

            return result;
        }

        // The bytes the ring uploaded for the frame
        ase::Buffer const& ringUpload() const
        {
            for (auto const& command : uploads)
            {
                auto const* upload = std::get_if<ase::BufferUploadCommand>(
                        &command);
                if (upload
                        && std::holds_alternative<ase::UniformBuffer>(
                            upload->target))
                {
                    return upload->data;
                }
            }

            throw std::logic_error("the ring uploaded nothing");
        }

        std::map<int, ase::UniformBufferRange> const& bindings(
                ase::DrawCommand const& draw) const
        {
            return draw.getUniforms().getImpl<ase::DummyUniformSet>()
                .getBindings();
        }

        PassBlock passBlock(ase::DrawCommand const& draw) const
        {
            ase::UniformBufferRange const& range = bindings(draw).at(0);
            ase::Buffer const& data = ringUpload();
            EXPECT_EQ(sizeof(PassBlock), range.size);
            EXPECT_LE(range.offset + range.size, data.getSize());

            PassBlock block;
            std::memcpy(&block, static_cast<char const*>(data.data())
                    + range.offset, sizeof(PassBlock));
            return block;
        }
    };

    void expectRegion(PixelRect const& expected, float const* actual)
    {
        EXPECT_EQ(static_cast<float>(expected.x), actual[0]);
        EXPECT_EQ(static_cast<float>(expected.y), actual[1]);
        EXPECT_EQ(static_cast<float>(expected.width), actual[2]);
        EXPECT_EQ(static_cast<float>(expected.height), actual[3]);
    }

    void expectTexel(Vector2i size, Divisor divisor, float const* actual)
    {
        EXPECT_FLOAT_EQ(1.0f / static_cast<float>(size[0]), actual[0]);
        EXPECT_FLOAT_EQ(1.0f / static_cast<float>(size[1]), actual[1]);
        EXPECT_EQ(static_cast<float>(avg::toInt(divisor)), actual[2]);
        EXPECT_EQ(0.0f, actual[3]);
    }

    void expectZeroTexel(float const* actual)
    {
        for (size_t i = 0; i < 4; ++i)
            EXPECT_EQ(0.0f, actual[i]);
    }

    void expectScissor(PixelRect const& expected, ase::DrawCommand const& draw)
    {
        ASSERT_TRUE(draw.getScissor().has_value());
        EXPECT_EQ(expected.x, draw.getScissor()->x);
        EXPECT_EQ(expected.y, draw.getScissor()->y);
        EXPECT_EQ(expected.width, draw.getScissor()->width);
        EXPECT_EQ(expected.height, draw.getScissor()->height);
    }

    bool isBarrier(ase::RenderCommand const* command)
    {
        return std::holds_alternative<ase::BarrierCommand>(*command);
    }

    bool isDraw(ase::RenderCommand const* command)
    {
        return std::holds_alternative<ase::DrawCommand>(*command);
    }
} // anonymous namespace

TEST(EffectEmitter, frostedGlassDrawsSnapshotAndEveryPassBetweenBarriers)
{
    Fixture f;
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));
    EffectRegions regions = avg::deriveRegions(glass, glassBounds, screenSize);

    ScratchTimeline timeline;
    size_t snapshot = timeline.addSnapshot();
    timeline.addEffect(glass, snapshot);
    ScratchPlan plan = avg::planScratch(timeline);

    f.beginFrame();
    avg::EffectEmitter emitter = f.emitter();
    emitter.emitSnapshot(plan.snapshots[0].slot, regions.backdropRegion);
    emitter.emitEffect(glass, regions, plan.effects[0],
            triangles(glassBounds));
    f.endFrame();

    EXPECT_EQ(1u, emitter.getSnapshotCount());
    EXPECT_EQ(4u, emitter.getPassCount());

    // barrier, snapshot, barrier, then draw + barrier per pass
    auto structure = f.structure();
    ASSERT_EQ(11u, structure.size());
    for (size_t i = 0; i < structure.size(); ++i)
    {
        if (i % 2 == 0)
            EXPECT_TRUE(isBarrier(structure[i])) << "command " << i;
        else
            EXPECT_TRUE(isDraw(structure[i])) << "command " << i;
    }

    auto draws = f.draws();
    ASSERT_EQ(5u, draws.size());

    // The snapshot copies the backdrop union from the main target into
    // the divisor-1 slot.
    ScratchSlot const snapshotSlot = slot(Divisor::One, ase::FORMAT_SRGBA, 0);
    EXPECT_EQ(snapshotSlot, plan.snapshots[0].slot);
    avg::ScratchTarget snapshotTarget = f.pool.acquire(snapshotSlot);
    {
        ase::DrawCommand const& draw = *draws[0];
        EXPECT_EQ(snapshotTarget.framebuffer, draw.getFramebuffer());
        ASSERT_EQ(1u, draw.getTextures().size());
        EXPECT_EQ(f.target.getTexture(), draw.getTextures()[0]);
        expectScissor(rect(50, 30, 300, 140), draw);
        EXPECT_FALSE(draw.getPipeline().isBlendEnabled());
        EXPECT_FALSE(draw.getPipeline().isDepthTestEnabled());

        PassBlock block = f.passBlock(draw);
        EXPECT_EQ(400.0f, block.targetSize[0]);
        EXPECT_EQ(300.0f, block.targetSize[1]);
        EXPECT_EQ(1.0f, block.targetDivisor);
        expectRegion(rect(50, 30, 300, 140), block.region);
        expectTexel(screenSize, Divisor::One, block.texel0);
        expectZeroTexel(block.texel1);
        EXPECT_EQ(0u, f.bindings(draw).count(1));
    }

    // The copy pass reads the snapshot into the first blur texture at
    // divisor 2.
    ScratchKey const blurKey{ Divisor::Two, ase::FORMAT_RGBA16F };
    Vector2i const blurSize(200, 150);
    auto const& passes = plan.effects[0].passes;
    ASSERT_EQ(4u, passes.size());
    avg::ScratchTarget copyTarget = f.pool.acquire(passes[0].output->slot);
    EXPECT_EQ(blurKey, passes[0].output->slot.key);
    EXPECT_EQ(blurSize, copyTarget.size);
    {
        ase::DrawCommand const& draw = *draws[1];
        EXPECT_EQ(copyTarget.framebuffer, draw.getFramebuffer());
        ASSERT_EQ(1u, draw.getTextures().size());
        EXPECT_EQ(snapshotTarget.texture, draw.getTextures()[0]);
        expectScissor(rect(25, 15, 150, 70), draw);
        EXPECT_FALSE(draw.getPipeline().isBlendEnabled());
        EXPECT_FALSE(draw.getPipeline().isDepthTestEnabled());
        EXPECT_FALSE(draw.getPipeline().isDepthWriteEnabled());

        PassBlock block = f.passBlock(draw);
        EXPECT_EQ(200.0f, block.targetSize[0]);
        EXPECT_EQ(150.0f, block.targetSize[1]);
        EXPECT_EQ(2.0f, block.targetDivisor);
        expectRegion(rect(25, 15, 150, 70), block.region);
        expectTexel(screenSize, Divisor::One, block.texel0);
        expectZeroTexel(block.texel1);
        EXPECT_EQ(0u, f.bindings(draw).count(1));
    }

    // The blurs ping-pong between the two blur textures and carry their
    // parameters at binding 1.
    avg::ScratchTarget blurHTarget = f.pool.acquire(passes[1].output->slot);
    avg::ScratchTarget blurVTarget = f.pool.acquire(passes[2].output->slot);
    EXPECT_NE(copyTarget.texture, blurHTarget.texture);
    EXPECT_EQ(copyTarget.texture, blurVTarget.texture);
    {
        ase::DrawCommand const& draw = *draws[2];
        EXPECT_EQ(blurHTarget.framebuffer, draw.getFramebuffer());
        ASSERT_EQ(1u, draw.getTextures().size());
        EXPECT_EQ(copyTarget.texture, draw.getTextures()[0]);
        expectScissor(rect(37, 27, 126, 46), draw);

        PassBlock block = f.passBlock(draw);
        EXPECT_EQ(2.0f, block.targetDivisor);
        expectRegion(rect(37, 27, 126, 46), block.region);
        expectTexel(blurSize, Divisor::Two, block.texel0);

        ASSERT_EQ(1u, f.bindings(draw).count(1));
        EXPECT_EQ(glass.getPasses()[1].parameters->getSize(),
                f.bindings(draw).at(1).size);
    }
    {
        ase::DrawCommand const& draw = *draws[3];
        EXPECT_EQ(blurVTarget.framebuffer, draw.getFramebuffer());
        ASSERT_EQ(1u, draw.getTextures().size());
        EXPECT_EQ(blurHTarget.texture, draw.getTextures()[0]);
        expectScissor(rect(49, 39, 102, 22), draw);
        expectRegion(rect(49, 39, 102, 22), f.passBlock(draw).region);
    }

    // The Shape pass blends the blurred result into the main target.
    {
        ase::DrawCommand const& draw = *draws[4];
        EXPECT_EQ(f.target.getFramebuffer(), draw.getFramebuffer());
        ASSERT_EQ(1u, draw.getTextures().size());
        EXPECT_EQ(blurVTarget.texture, draw.getTextures()[0]);
        expectScissor(glassBounds, draw);
        EXPECT_TRUE(draw.getPipeline().isBlendEnabled());
        EXPECT_FALSE(draw.getPipeline().isDepthTestEnabled());
        EXPECT_FALSE(draw.getPipeline().isDepthWriteEnabled());

        PassBlock block = f.passBlock(draw);
        EXPECT_EQ(400.0f, block.targetSize[0]);
        EXPECT_EQ(300.0f, block.targetSize[1]);
        EXPECT_EQ(1.0f, block.targetDivisor);
        expectRegion(glassBounds, block.region);
        expectTexel(blurSize, Divisor::Two, block.texel0);
        ASSERT_EQ(1u, f.bindings(draw).count(1));
        EXPECT_EQ(glass.getPasses()[3].parameters->getSize(),
                f.bindings(draw).at(1).size);
    }

    // Each draw binds its own set, so no draw sees another's ranges.
    for (size_t i = 0; i < draws.size(); ++i)
    {
        for (size_t j = i + 1; j < draws.size(); ++j)
            EXPECT_NE(draws[i]->getUniforms(), draws[j]->getUniforms());
    }
}

TEST(EffectEmitter, tintReadsTheSnapshotInItsOnlyPass)
{
    Fixture f;
    Effect dim = avg::tint(avg::Color(0.0f, 0.0f, 0.0f), 0.5f);
    EffectRegions regions = avg::deriveRegions(dim, glassBounds, screenSize);

    ScratchTimeline timeline;
    size_t snapshot = timeline.addSnapshot();
    timeline.addEffect(dim, snapshot);
    ScratchPlan plan = avg::planScratch(timeline);

    f.beginFrame();
    avg::EffectEmitter emitter = f.emitter();
    emitter.emitSnapshot(plan.snapshots[0].slot, regions.backdropRegion);
    emitter.emitEffect(dim, regions, plan.effects[0], triangles(glassBounds));
    f.endFrame();

    auto draws = f.draws();
    ASSERT_EQ(2u, draws.size());
    expectScissor(glassBounds, *draws[0]);

    ase::DrawCommand const& draw = *draws[1];
    EXPECT_EQ(f.target.getFramebuffer(), draw.getFramebuffer());
    ASSERT_EQ(1u, draw.getTextures().size());
    EXPECT_EQ(f.pool.acquire(plan.snapshots[0].slot).texture,
            draw.getTextures()[0]);
    EXPECT_TRUE(draw.getPipeline().isBlendEnabled());
    expectTexel(screenSize, Divisor::One, f.passBlock(draw).texel0);
    EXPECT_EQ(1u, f.bindings(draw).count(1));
}

TEST(EffectEmitter, offscreenEffectRecordsNothing)
{
    Fixture f;
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));
    PixelRect const outside = rect(500, 400, 50, 50);
    EffectRegions regions = avg::deriveRegions(glass, outside, screenSize);

    ScratchTimeline timeline;
    size_t snapshot = timeline.addSnapshot();
    timeline.addEffect(glass, snapshot);
    ScratchPlan plan = avg::planScratch(timeline);

    f.beginFrame();
    avg::EffectEmitter emitter = f.emitter();
    emitter.emitSnapshot(plan.snapshots[0].slot, regions.backdropRegion);
    emitter.emitEffect(glass, regions, plan.effects[0], triangles(outside));
    f.endFrame();

    EXPECT_EQ(0u, f.commands.size());
    EXPECT_EQ(0u, emitter.getPassCount());
    EXPECT_EQ(0u, f.pool.getTextureCount());
}

TEST(EffectEmitter, snapshotPipelineDeclaresTheScratchFormat)
{
    Fixture f;
    avg::TargetImage floatTarget{ f.context, screenSize,
        ase::FORMAT_RGBA16F };

    f.beginFrame();
    avg::EffectEmitter emitter(f.context, f.commands, f.ring, f.pool,
            f.cache, floatTarget);
    emitter.emitSnapshot(slot(Divisor::One, ase::FORMAT_SRGBA, 0),
            glassBounds);
    f.endFrame();

    auto draws = f.draws();
    ASSERT_EQ(1u, draws.size());
    EXPECT_TRUE(draws[0]->getPipeline().matchesTarget(
                draws[0]->getFramebuffer()));
}

TEST(EffectEmitter, rejectsMismatchedRegions)
{
    Fixture f;
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));
    Effect dim = avg::tint(avg::Color(0.0f, 0.0f, 0.0f), 0.5f);
    EffectRegions regions = avg::deriveRegions(dim, glassBounds, screenSize);

    ScratchTimeline timeline;
    size_t snapshot = timeline.addSnapshot();
    timeline.addEffect(glass, snapshot);
    ScratchPlan plan = avg::planScratch(timeline);

    f.beginFrame();
    avg::EffectEmitter emitter = f.emitter();
    EXPECT_THROW(emitter.emitEffect(glass, regions, plan.effects[0],
                triangles(glassBounds)), std::invalid_argument);
    f.endFrame();
}

TEST(ScratchPool, keepsTexturesAcrossFramesAndRemakesThemOnResize)
{
    Fixture f;
    ScratchSlot const blur = slot(Divisor::Two, ase::FORMAT_RGBA16F, 0);

    EXPECT_THROW(f.pool.acquire(blur), std::logic_error);

    f.pool.beginFrame(screenSize);
    avg::ScratchTarget first = f.pool.acquire(blur);
    f.pool.endFrame();
    EXPECT_EQ(Vector2i(200, 150), first.size);
    EXPECT_EQ(Vector2i(200, 150), first.texture.getSize());
    EXPECT_EQ(ase::FORMAT_RGBA16F, first.texture.getFormat());

    f.pool.beginFrame(screenSize);
    avg::ScratchTarget again = f.pool.acquire(blur);
    f.pool.endFrame();
    EXPECT_EQ(first.texture, again.texture);
    EXPECT_EQ(first.framebuffer, again.framebuffer);
    EXPECT_EQ(1u, f.pool.getTextureCount());

    f.pool.beginFrame(Vector2i(801, 601));
    avg::ScratchTarget resized = f.pool.acquire(blur);
    f.pool.endFrame();
    EXPECT_NE(first.texture, resized.texture);
    EXPECT_EQ(Vector2i(401, 301), resized.size);
    EXPECT_EQ(Vector2i(401, 301), resized.texture.getSize());
    EXPECT_EQ(1u, f.pool.getTextureCount());
}

TEST(ScratchPool, slotsOfOneKeyAreDistinctTextures)
{
    Fixture f;
    f.pool.beginFrame(screenSize);
    avg::ScratchTarget a = f.pool.acquire(slot(Divisor::Two,
                ase::FORMAT_RGBA16F, 0));
    avg::ScratchTarget b = f.pool.acquire(slot(Divisor::Two,
                ase::FORMAT_RGBA16F, 1));
    avg::ScratchTarget c = f.pool.acquire(slot(Divisor::Two,
                ase::FORMAT_SRGBA, 0));
    f.pool.endFrame();

    EXPECT_NE(a.texture, b.texture);
    EXPECT_NE(a.texture, c.texture);
    EXPECT_EQ(ase::FORMAT_SRGBA, c.texture.getFormat());
    EXPECT_EQ(3u, f.pool.getTextureCount());
}

TEST(ScratchPool, releasesATextureIdleForTheLimit)
{
    Fixture f;
    ScratchSlot const kept = slot(Divisor::Two, ase::FORMAT_RGBA16F, 0);
    ScratchSlot const idle = slot(Divisor::Two, ase::FORMAT_RGBA16F, 1);

    f.pool.beginFrame(screenSize);
    f.pool.acquire(kept);
    avg::ScratchTarget before = f.pool.acquire(idle);
    f.pool.endFrame();

    for (size_t i = 1; i < avg::scratchIdleFrameLimit; ++i)
    {
        f.pool.beginFrame(screenSize);
        f.pool.acquire(kept);
        f.pool.endFrame();
        EXPECT_EQ(2u, f.pool.getTextureCount()) << "frame " << i;
    }

    f.pool.beginFrame(screenSize);
    f.pool.acquire(kept);
    f.pool.endFrame();
    EXPECT_EQ(1u, f.pool.getTextureCount());

    f.pool.beginFrame(screenSize);
    EXPECT_NE(before.texture, f.pool.acquire(idle).texture);
    f.pool.endFrame();
}

TEST(PassPipelineCache, returnsOnePipelinePerShaderFormatAndGeometry)
{
    Fixture f;
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));
    avg::Pass const& copy = glass.getPasses()[0];
    avg::Pass const& blurH = glass.getPasses()[1];
    avg::Pass const& blurV = glass.getPasses()[2];

    avg::PassPipeline& first = f.cache.getPipeline(copy.shader,
            Geometry::Region, ase::FORMAT_RGBA16F, std::nullopt);
    avg::PassPipeline& again = f.cache.getPipeline(copy.shader,
            Geometry::Region, ase::FORMAT_RGBA16F, std::nullopt);
    EXPECT_EQ(&first, &again);
    EXPECT_EQ(first.getPipeline(), again.getPipeline());
    EXPECT_EQ(1u, f.cache.getPipelineCount());
    EXPECT_EQ(0, first.getPassBlockBinding());
    EXPECT_FALSE(first.getParamBlockBinding().has_value());
    EXPECT_EQ(1u, first.getInputCount());

    // The two blurs share a source and differ only in parameters.
    avg::PassPipeline& blur = f.cache.getPipeline(blurH.shader,
            Geometry::Region, ase::FORMAT_RGBA16F, std::nullopt);
    EXPECT_EQ(&blur, &f.cache.getPipeline(blurV.shader, Geometry::Region,
                ase::FORMAT_RGBA16F, std::nullopt));
    EXPECT_EQ(2u, f.cache.getPipelineCount());
    ASSERT_TRUE(blur.getParamBlockBinding().has_value());
    EXPECT_EQ(1, *blur.getParamBlockBinding());

    EXPECT_NE(first.getPipeline(), f.cache.getPipeline(copy.shader,
                Geometry::Region, ase::FORMAT_SRGBA, std::nullopt)
            .getPipeline());
    EXPECT_NE(first.getPipeline(), f.cache.getPipeline(copy.shader,
                Geometry::Shape, ase::FORMAT_RGBA16F, std::nullopt)
            .getPipeline());
    EXPECT_NE(first.getPipeline(), f.cache.getPipeline(copy.shader,
                Geometry::Region, ase::FORMAT_RGBA16F,
                avg::TargetImage::depthFormat).getPipeline());
    EXPECT_EQ(5u, f.cache.getPipelineCount());
}

TEST(PassPipelineCache, sharesOneProgramAcrossTargetFormats)
{
    Fixture f;
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));
    avg::PassShader const& shader = glass.getPasses()[0].shader;

    ase::Pipeline const& half = f.cache.getPipeline(shader, Geometry::Region,
            ase::FORMAT_RGBA16F, std::nullopt).getPipeline();
    ase::Pipeline const& full = f.cache.getPipeline(shader, Geometry::Region,
            ase::FORMAT_SRGBA, std::nullopt).getPipeline();
    ase::Pipeline const& onMain = f.cache.getPipeline(shader, Geometry::Shape,
            ase::FORMAT_SRGBA, avg::TargetImage::depthFormat).getPipeline();

    EXPECT_NE(half, full);
    EXPECT_NE(full, onMain);
    EXPECT_EQ(half.getProgram(), full.getProgram());
    EXPECT_EQ(half.getProgram(), onMain.getProgram());
    EXPECT_EQ(3u, f.cache.getPipelineCount());

    EXPECT_EQ(ase::FORMAT_RGBA16F, half.getColorFormat());
    EXPECT_FALSE(half.getDepthFormat().has_value());
    EXPECT_EQ(ase::FORMAT_SRGBA, onMain.getColorFormat());
    EXPECT_EQ(avg::TargetImage::depthFormat, onMain.getDepthFormat());
}

TEST(PassPipelineCache, handsOutUniformSetsPerDrawAndAgainEachFrame)
{
    Fixture f;
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));
    avg::PassPipeline& pipeline = f.cache.getPipeline(
            glass.getPasses()[0].shader, Geometry::Region,
            ase::FORMAT_RGBA16F, std::nullopt);

    f.cache.beginFrame();
    ase::UniformSet a = pipeline.acquireUniformSet();
    ase::UniformSet b = pipeline.acquireUniformSet();
    EXPECT_NE(a, b);

    f.cache.beginFrame();
    EXPECT_EQ(a, pipeline.acquireUniformSet());
    EXPECT_EQ(b, pipeline.acquireUniformSet());
    EXPECT_NE(a, pipeline.acquireUniformSet());
}

TEST(PassPipelineCache, pipelinesDrawWithDepthOffAndBlendOnlyForShapes)
{
    Fixture f;
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));

    ase::Pipeline const& region = f.cache.getPipeline(
            glass.getPasses()[0].shader, Geometry::Region,
            ase::FORMAT_RGBA16F, std::nullopt).getPipeline();
    EXPECT_FALSE(region.isBlendEnabled());
    EXPECT_FALSE(region.isDepthTestEnabled());
    EXPECT_FALSE(region.isDepthWriteEnabled());

    ase::Pipeline const& shape = f.cache.getPipeline(
            glass.getFinalPass().shader, Geometry::Shape,
            ase::FORMAT_SRGBA, avg::TargetImage::depthFormat).getPipeline();
    EXPECT_TRUE(shape.isBlendEnabled());
    EXPECT_FALSE(shape.isDepthTestEnabled());
    EXPECT_FALSE(shape.isDepthWriteEnabled());
}
