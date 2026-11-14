#include <avg/effect.h>
#include <avg/effectplan.h>
#include <avg/effectshader.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

using avg::Divisor;
using avg::Effect;
using avg::EffectRegions;
using avg::Geometry;
using avg::Pass;
using avg::PassInput;
using avg::PassOutput;
using avg::PixelRect;
using avg::ScratchKey;
using avg::ScratchPlan;
using avg::ScratchSlot;
using avg::ScratchTimeline;
using avg::Vector2i;

namespace
{
    ase::ShaderInterface samplers(size_t count)
    {
        ase::ShaderInterface iface;
        for (size_t i = 0; i < count; ++i)
            iface.sampler("input" + std::to_string(i), static_cast<int>(i));

        return iface;
    }

    Pass regionPass(std::vector<PassInput> inputs, Divisor divisor,
            ase::Format format, int padding = 0)
    {
        Pass pass;
        pass.shader.interface = samplers(inputs.size());
        pass.inputs = std::move(inputs);
        pass.output = PassOutput{ divisor, format };
        pass.geometry = Geometry::Region;
        pass.padding = padding;
        return pass;
    }

    Pass finalPass(std::vector<PassInput> inputs, int padding = 0)
    {
        Pass pass;
        pass.shader.interface = samplers(inputs.size());
        pass.inputs = std::move(inputs);
        pass.geometry = Geometry::Shape;
        pass.padding = padding;
        return pass;
    }

    PixelRect rect(int x, int y, int width, int height)
    {
        PixelRect r;
        r.x = x;
        r.y = y;
        r.width = width;
        r.height = height;
        return r;
    }

    ScratchKey key(Divisor divisor, ase::Format format)
    {
        ScratchKey k;
        k.divisor = divisor;
        k.format = format;
        return k;
    }

    ScratchKey const snapshotKey = key(Divisor::One, ase::FORMAT_SRGBA);
    ScratchKey const halfKey = key(Divisor::Two, ase::FORMAT_RGBA16F);

    // Downsample the backdrop to half size, blur it in two separable
    // passes of radius 4, then draw the shape over the result.
    std::vector<Pass> blurPasses()
    {
        return {
            regionPass({ PassInput::backdrop() }, Divisor::Two,
                    ase::FORMAT_RGBA16F),
            regionPass({ PassInput::pass(0) }, Divisor::Two,
                    ase::FORMAT_RGBA16F, 4),
            regionPass({ PassInput::pass(1) }, Divisor::Two,
                    ase::FORMAT_RGBA16F, 4),
            finalPass({ PassInput::pass(2), PassInput::backdrop() })
        };
    }

    bool mentions(std::vector<std::string> const& errors,
            std::string const& text)
    {
        for (std::string const& error : errors)
        {
            if (error.find(text) != std::string::npos)
                return true;
        }

        return false;
    }
} // anonymous namespace

TEST(Effect, acceptsTwoPassBlur)
{
    EXPECT_TRUE(avg::validate(blurPasses()).empty());

    Effect effect(blurPasses());
    EXPECT_EQ(4u, effect.getPasses().size());
    EXPECT_EQ(Geometry::Shape, effect.getFinalPass().geometry);
}

TEST(Effect, acceptsPlainTint)
{
    EXPECT_TRUE(avg::validate({ finalPass({ PassInput::backdrop() }) })
            .empty());
}

TEST(Effect, rejectsNoPasses)
{
    EXPECT_TRUE(mentions(avg::validate({}), "at least one pass"));
}

TEST(Effect, rejectsFinalPassThatIsNotTheShape)
{
    auto passes = blurPasses();
    passes.pop_back();

    EXPECT_TRUE(mentions(avg::validate(passes), "must draw the shape"));
}

TEST(Effect, rejectsShapePassBeforeTheEnd)
{
    auto passes = blurPasses();
    passes[2].geometry = Geometry::Shape;
    passes[2].output.reset();

    EXPECT_TRUE(mentions(avg::validate(passes), "not the final pass"));
}

TEST(Effect, rejectsReadingALaterPass)
{
    auto passes = blurPasses();
    passes[1].inputs = { PassInput::pass(2) };

    EXPECT_TRUE(mentions(avg::validate(passes), "runs after it"));
}

TEST(Effect, rejectsReadingItsOwnOutput)
{
    auto passes = blurPasses();
    passes[1].inputs = { PassInput::pass(1) };

    EXPECT_TRUE(mentions(avg::validate(passes), "reads its own output"));
}

TEST(Effect, rejectsDivisorOutsideTheSet)
{
    auto passes = blurPasses();
    passes[0].output->divisor = static_cast<Divisor>(3);

    EXPECT_TRUE(mentions(avg::validate(passes), "divisor 3"));
}

TEST(Effect, rejectsNonColourFormat)
{
    auto passes = blurPasses();
    passes[0].output->format = ase::FORMAT_DEPTH24;

    EXPECT_TRUE(mentions(avg::validate(passes), "has format"));
}

TEST(Effect, rejectsRegionPassWithoutOutput)
{
    auto passes = blurPasses();
    passes[0].output.reset();

    EXPECT_TRUE(mentions(avg::validate(passes), "has no output"));
}

TEST(Effect, rejectsShapePassWithOutput)
{
    auto passes = blurPasses();
    passes[3].output = PassOutput{};

    EXPECT_TRUE(mentions(avg::validate(passes), "cannot have an output"));
}

TEST(Effect, rejectsOutputNobodyReads)
{
    auto passes = blurPasses();
    passes[3].inputs = { PassInput::pass(1) };

    auto errors = avg::validate(passes);
    EXPECT_TRUE(mentions(errors, "pass 2's output is never read"));
    EXPECT_FALSE(mentions(errors, "pass 1's output is never read"));
}

TEST(Effect, rejectsNegativePadding)
{
    auto passes = blurPasses();
    passes[1].padding = -1;

    EXPECT_TRUE(mentions(avg::validate(passes), "negative padding"));
}

TEST(Effect, rejectsParametersForAnUndeclaredBlock)
{
    ase::ShaderInterface declared = samplers(1);
    declared.block("ParamBlock", 1, ase::BlockLayout::Std140, {
            { "tint", ase::UniformType::Float4 },
            });

    std::vector<Pass> passes = { finalPass({ PassInput::backdrop() }) };
    passes[0].parameters = ase::UniformValues(declared, "ParamBlock");

    EXPECT_TRUE(mentions(avg::validate(passes), "does not declare"));

    passes[0].shader.interface = declared;
    EXPECT_TRUE(avg::validate(passes).empty());
}

TEST(Effect, rejectsParametersWhoseLayoutDiffers)
{
    ase::ShaderInterface declared = samplers(1);
    declared.block("ParamBlock", 1, ase::BlockLayout::Std140, {
            { "tint", ase::UniformType::Float4 },
            });

    ase::ShaderInterface other;
    other.block("ParamBlock", 1, ase::BlockLayout::Std140, {
            { "tint", ase::UniformType::Float4 },
            { "radius", ase::UniformType::Float },
            });

    std::vector<Pass> passes = { finalPass({ PassInput::backdrop() }) };
    passes[0].shader.interface = declared;
    passes[0].parameters = ase::UniformValues(other, "ParamBlock");

    EXPECT_TRUE(mentions(avg::validate(passes), "layout differs"));
}

TEST(Effect, rejectsMoreInputsThanAPassCanRead)
{
    std::vector<PassInput> const inputs(avg::maxPassInputs + 1,
            PassInput::backdrop());

    EXPECT_TRUE(avg::validate({ finalPass(std::vector<PassInput>(
                        avg::maxPassInputs, PassInput::backdrop())) })
            .empty());
    EXPECT_TRUE(mentions(avg::validate({ finalPass(inputs) }),
                "the most is"));
}

TEST(Effect, rejectsAShaderWithoutASamplerPerInput)
{
    std::vector<Pass> passes = { finalPass({ PassInput::backdrop() }) };

    passes[0].shader.interface = samplers(0);
    EXPECT_TRUE(mentions(avg::validate(passes), "declares 0 samplers"));

    passes[0].shader.interface = samplers(2);
    EXPECT_TRUE(mentions(avg::validate(passes), "declares 2 samplers"));
}

TEST(Effect, constructorThrowsWithEveryError)
{
    auto passes = blurPasses();
    passes[1].padding = -1;
    passes[0].output->divisor = static_cast<Divisor>(5);

    try
    {
        Effect effect(passes);
        FAIL() << "expected std::invalid_argument";
    }
    catch (std::invalid_argument const& e)
    {
        std::string const message = e.what();
        EXPECT_NE(std::string::npos, message.find("negative padding"));
        EXPECT_NE(std::string::npos, message.find("divisor 5"));
    }
}

TEST(PixelRect, operations)
{
    EXPECT_TRUE(PixelRect().isEmpty());
    EXPECT_FALSE(rect(0, 0, 1, 1).isEmpty());

    EXPECT_EQ(rect(2, 3, 8, 7), rect(2, 3, 4, 4).unionWith(rect(5, 6, 5, 4)));
    EXPECT_EQ(rect(2, 3, 4, 4), rect(2, 3, 4, 4).unionWith(PixelRect()));
    EXPECT_EQ(rect(2, 3, 4, 4), PixelRect().unionWith(rect(2, 3, 4, 4)));

    EXPECT_EQ(rect(0, 1, 8, 8), rect(2, 3, 4, 4).grown(2));
    EXPECT_EQ(PixelRect(), PixelRect().grown(2));

    EXPECT_EQ(rect(0, 0, 10, 10), rect(-5, -5, 15, 15).clamped(
                Vector2i(10, 10)));
    EXPECT_EQ(PixelRect(), rect(20, 20, 5, 5).clamped(Vector2i(10, 10)));
}

TEST(PixelRect, rescalingRoundsOutwards)
{
    // [3, 10) x [5, 12) at full size covers [1, 5) x [2, 6) at half size.
    EXPECT_EQ(rect(1, 2, 4, 4), rect(3, 5, 7, 7).rescaled(Divisor::One,
                Divisor::Two));
    EXPECT_EQ(rect(2, 4, 8, 8), rect(1, 2, 4, 4).rescaled(Divisor::Two,
                Divisor::One));
    EXPECT_EQ(rect(-2, 0, 2, 1), rect(-3, 0, 3, 1).rescaled(Divisor::One,
                Divisor::Two));
    EXPECT_EQ(rect(1, 2, 1, 1), rect(2, 4, 2, 2).rescaled(Divisor::Two,
                Divisor::Four));
}

TEST(EffectPlan, targetSizeRoundsUp)
{
    EXPECT_EQ(Vector2i(961, 541), avg::targetSize(Vector2i(1921, 1081),
                Divisor::Two));
    EXPECT_EQ(Vector2i(481, 271), avg::targetSize(Vector2i(1921, 1081),
                Divisor::Four));
    EXPECT_EQ(Vector2i(100, 50), avg::targetSize(Vector2i(200, 100),
                Divisor::Two));
}

TEST(EffectPlan, regionsGrowByPaddingAndClampToEachTarget)
{
    Effect blur(blurPasses());

    // The shape runs 10 px past the right edge of a 200x100 screen.
    EffectRegions regions = avg::deriveRegions(blur, rect(150, 40, 60, 30),
            Vector2i(200, 100));

    ASSERT_EQ(4u, regions.passRegions.size());
    EXPECT_EQ(rect(150, 40, 50, 30), regions.passRegions[3]);
    EXPECT_EQ(rect(75, 20, 25, 15), regions.passRegions[2]);
    EXPECT_EQ(rect(71, 16, 29, 23), regions.passRegions[1]);
    EXPECT_EQ(rect(67, 12, 33, 31), regions.passRegions[0]);
    EXPECT_EQ(rect(134, 24, 66, 62), regions.backdropRegion);
}

TEST(EffectPlan, regionsAtOddSizesCoverTheWholeScreen)
{
    Effect effect({
        regionPass({ PassInput::backdrop() }, Divisor::Two,
                ase::FORMAT_SRGBA),
        finalPass({ PassInput::pass(0) })
    });

    EffectRegions regions = avg::deriveRegions(effect, rect(0, 0, 201, 101),
            Vector2i(201, 101));

    EXPECT_EQ(rect(0, 0, 201, 101), regions.passRegions[1]);
    EXPECT_EQ(rect(0, 0, 101, 51), regions.passRegions[0]);
    EXPECT_EQ(rect(0, 0, 201, 101), regions.backdropRegion);
}

TEST(EffectPlan, regionsOfAnOffscreenShapeAreEmpty)
{
    Effect blur(blurPasses());

    EffectRegions regions = avg::deriveRegions(blur, rect(-50, -50, 20, 20),
            Vector2i(200, 100));

    for (PixelRect const& region : regions.passRegions)
        EXPECT_TRUE(region.isEmpty());
    EXPECT_TRUE(regions.backdropRegion.isEmpty());
}

TEST(EffectPlan, regionsClampAtTheLeftEdge)
{
    Effect tint({ finalPass({ PassInput::backdrop() }) });

    EffectRegions regions = avg::deriveRegions(tint, rect(-10, 5, 30, 10),
            Vector2i(100, 100));

    EXPECT_EQ(rect(0, 5, 20, 10), regions.passRegions[0]);
    EXPECT_EQ(rect(0, 5, 20, 10), regions.backdropRegion);
}

TEST(EffectPlan, backdropRegionIsEmptyWhenNothingReadsIt)
{
    Effect fill({ finalPass({}) });

    EffectRegions regions = avg::deriveRegions(fill, rect(10, 10, 10, 10),
            Vector2i(100, 100));

    EXPECT_EQ(rect(10, 10, 10, 10), regions.passRegions[0]);
    EXPECT_TRUE(regions.backdropRegion.isEmpty());
}

TEST(ScratchPlan, chainPingPongsBetweenTwoSlots)
{
    Effect blur(blurPasses());
    ScratchTimeline timeline;
    size_t const snapshot = timeline.addSnapshot();
    timeline.addEffect(blur, snapshot);

    ScratchPlan plan = avg::planScratch(timeline);

    ASSERT_EQ(1u, plan.effects.size());
    auto const& passes = plan.effects[0].passes;
    ASSERT_EQ(4u, passes.size());

    EXPECT_EQ(2u, plan.getPoolSize(halfKey));
    EXPECT_EQ(1u, plan.getPoolSize(snapshotKey));

    ASSERT_TRUE(passes[0].output.has_value());
    ASSERT_TRUE(passes[1].output.has_value());
    ASSERT_TRUE(passes[2].output.has_value());
    EXPECT_FALSE(passes[3].output.has_value());

    EXPECT_EQ((ScratchSlot{ halfKey, 0 }), passes[0].output->slot);
    EXPECT_EQ((ScratchSlot{ halfKey, 1 }), passes[1].output->slot);
    EXPECT_EQ((ScratchSlot{ halfKey, 0 }), passes[2].output->slot);

    for (auto const& pass : passes)
    {
        if (!pass.output)
            continue;

        for (ScratchSlot const& input : pass.inputs)
            EXPECT_NE(pass.output->slot, input);
    }

    EXPECT_EQ(passes[1].output->slot, passes[2].inputs[0]);
    EXPECT_EQ(passes[2].output->slot, passes[3].inputs[0]);
}

TEST(ScratchPlan, snapshotStaysLiveUntilItsLastReader)
{
    Effect blur(blurPasses());
    ScratchTimeline timeline;
    size_t const snapshot = timeline.addSnapshot();
    timeline.addEffect(blur, snapshot);

    ScratchPlan plan = avg::planScratch(timeline);

    ASSERT_EQ(1u, plan.snapshots.size());
    EXPECT_EQ((ScratchSlot{ snapshotKey, 0 }), plan.snapshots[0].slot);
    EXPECT_EQ(0u, plan.snapshots[0].definedAt);
    // Steps: snapshot 0, passes 1..4; the final pass reads the backdrop.
    EXPECT_EQ(4u, plan.snapshots[0].lastUse);
    EXPECT_EQ(4u, plan.effects[0].passes[3].step);

    EXPECT_EQ(plan.snapshots[0].slot, plan.effects[0].passes[0].inputs[0]);
    EXPECT_EQ(plan.snapshots[0].slot, plan.effects[0].passes[3].inputs[1]);

    EXPECT_EQ(1u, plan.effects[0].passes[0].output->definedAt);
    EXPECT_EQ(2u, plan.effects[0].passes[0].output->lastUse);
}

TEST(ScratchPlan, consecutiveEffectsReuseTheSameSlots)
{
    Effect blur(blurPasses());
    ScratchTimeline timeline;
    size_t const snapshot = timeline.addSnapshot();
    timeline.addEffect(blur, snapshot);
    timeline.addEffect(blur, snapshot);

    ScratchPlan plan = avg::planScratch(timeline);

    ASSERT_EQ(2u, plan.effects.size());
    EXPECT_EQ(2u, plan.getPoolSize(halfKey));
    EXPECT_EQ(1u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ((ScratchSlot{ halfKey, 0 }),
            plan.effects[1].passes[0].output->slot);
    EXPECT_EQ(8u, plan.snapshots[0].lastUse);
}

TEST(ScratchPlan, outputNeverAliasesAnInputOfAWideGraph)
{
    Effect effect({
        regionPass({ PassInput::backdrop() }, Divisor::Two,
                ase::FORMAT_RGBA16F),
        regionPass({ PassInput::backdrop() }, Divisor::Two,
                ase::FORMAT_RGBA16F),
        regionPass({ PassInput::pass(0), PassInput::pass(1) }, Divisor::Two,
                ase::FORMAT_RGBA16F),
        finalPass({ PassInput::pass(2) })
    });
    ScratchTimeline timeline;
    timeline.addEffect(effect, timeline.addSnapshot());

    ScratchPlan plan = avg::planScratch(timeline);

    auto const& passes = plan.effects[0].passes;
    EXPECT_EQ(3u, plan.getPoolSize(halfKey));
    EXPECT_EQ((ScratchSlot{ halfKey, 2 }), passes[2].output->slot);
    EXPECT_EQ((ScratchSlot{ halfKey, 0 }), passes[2].inputs[0]);
    EXPECT_EQ((ScratchSlot{ halfKey, 1 }), passes[2].inputs[1]);
}

TEST(ScratchPlan, nestedScopesHoldTwoSnapshots)
{
    Effect tint({ finalPass({ PassInput::backdrop() }) });
    ScratchTimeline timeline;
    size_t const outer = timeline.addSnapshot();
    timeline.addEffect(tint, outer);
    size_t const inner = timeline.addSnapshot();
    timeline.addEffect(tint, inner);
    timeline.addEffect(tint, outer);

    ScratchPlan plan = avg::planScratch(timeline);

    EXPECT_EQ(2u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ((ScratchSlot{ snapshotKey, 0 }), plan.snapshots[outer].slot);
    EXPECT_EQ((ScratchSlot{ snapshotKey, 1 }), plan.snapshots[inner].slot);
    EXPECT_EQ(4u, plan.snapshots[outer].lastUse);
    EXPECT_EQ(3u, plan.snapshots[inner].lastUse);
    EXPECT_EQ(plan.snapshots[inner].slot, plan.effects[1].passes[0].inputs[0]);
}

TEST(ScratchPlan, unreadSnapshotIsReleasedAtOnce)
{
    ScratchTimeline timeline;
    timeline.addSnapshot();
    timeline.addSnapshot();

    ScratchPlan plan = avg::planScratch(timeline);

    EXPECT_EQ(1u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ(plan.snapshots[0].definedAt, plan.snapshots[0].lastUse);
    EXPECT_EQ(plan.snapshots[0].slot, plan.snapshots[1].slot);
}

TEST(ScratchPlan, poolsAreCountedPerKey)
{
    Effect effect({
        regionPass({ PassInput::backdrop() }, Divisor::Two,
                ase::FORMAT_RGBA16F),
        regionPass({ PassInput::pass(0) }, Divisor::Two, ase::FORMAT_R8),
        regionPass({ PassInput::pass(1) }, Divisor::One, ase::FORMAT_SRGBA),
        finalPass({ PassInput::pass(2), PassInput::backdrop() })
    });
    ScratchTimeline timeline;
    timeline.addEffect(effect, timeline.addSnapshot());

    ScratchPlan plan = avg::planScratch(timeline);

    EXPECT_EQ(3u, plan.pools.size());
    EXPECT_EQ(1u, plan.getPoolSize(halfKey));
    EXPECT_EQ(1u, plan.getPoolSize(key(Divisor::Two, ase::FORMAT_R8)));
    // The snapshot is still live when pass 2 writes at full size.
    EXPECT_EQ(2u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ(0u, plan.getPoolSize(key(Divisor::Eight, ase::FORMAT_R8)));
}

TEST(ScratchTimeline, rejectsAnUnknownSnapshot)
{
    Effect tint({ finalPass({ PassInput::backdrop() }) });
    ScratchTimeline timeline;

    EXPECT_THROW(timeline.addEffect(tint, 0), std::invalid_argument);
}
