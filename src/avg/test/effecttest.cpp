#include <effectfactory.h>

#include <avg/effect.h>
#include <avg/effectplan.h>
#include <avg/effectshader.h>

#include <ase/uniformvalues.h>
#include <ase/vector.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using avg::Divisor;
using avg::Effect;
using avg::EffectRegions;
using avg::Geometry;
using avg::Pass;
using avg::PassGraph;
using avg::PassInput;
using avg::PassOutput;
using avg::PixelRect;
using avg::ScratchKey;
using avg::ScratchPlan;
using avg::ScratchSlot;
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

    Pass withSource(Pass pass, std::string source)
    {
        pass.shader.fragmentSource = std::move(source);
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

    Vector2i const screen(400, 300);
    PixelRect const shape = rect(100, 80, 200, 40);

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

    Effect effect = avg::makeEffect(blurPasses());
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
        Effect effect = avg::makeEffect(passes);
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
    Effect blur = avg::makeEffect(blurPasses());

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
    Effect effect = avg::makeEffect({
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
    Effect blur = avg::makeEffect(blurPasses());

    EffectRegions regions = avg::deriveRegions(blur, rect(-50, -50, 20, 20),
            Vector2i(200, 100));

    for (PixelRect const& region : regions.passRegions)
        EXPECT_TRUE(region.isEmpty());
    EXPECT_TRUE(regions.backdropRegion.isEmpty());
}

TEST(EffectPlan, regionsClampAtTheLeftEdge)
{
    Effect tint = avg::makeEffect({ finalPass({ PassInput::backdrop() }) });

    EffectRegions regions = avg::deriveRegions(tint, rect(-10, 5, 30, 10),
            Vector2i(100, 100));

    EXPECT_EQ(rect(0, 5, 20, 10), regions.passRegions[0]);
    EXPECT_EQ(rect(0, 5, 20, 10), regions.backdropRegion);
}

TEST(EffectPlan, backdropRegionIsEmptyWhenNothingReadsIt)
{
    Effect fill = avg::makeEffect({ finalPass({}) });

    EffectRegions regions = avg::deriveRegions(fill, rect(10, 10, 10, 10),
            Vector2i(100, 100));

    EXPECT_EQ(rect(10, 10, 10, 10), regions.passRegions[0]);
    EXPECT_TRUE(regions.backdropRegion.isEmpty());
}

TEST(ScratchPlan, chainPingPongsBetweenTwoSlots)
{
    Effect blur = avg::makeEffect(blurPasses());
    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    graph.addEffect(blur, snapshot, shape);

    ScratchPlan plan = avg::planScratch(graph);

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
    Effect blur = avg::makeEffect(blurPasses());
    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    graph.addEffect(blur, snapshot, shape);

    ScratchPlan plan = avg::planScratch(graph);

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
    Effect blur = avg::makeEffect(blurPasses());
    auto passes = blurPasses();
    passes[0].shader.fragmentSource = "other";
    Effect other = avg::makeEffect(passes);
    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    graph.addEffect(blur, snapshot, shape);
    graph.addEffect(other, snapshot, shape);

    ScratchPlan plan = avg::planScratch(graph);

    ASSERT_EQ(2u, plan.effects.size());
    EXPECT_EQ(2u, plan.getPoolSize(halfKey));
    EXPECT_EQ(1u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ((ScratchSlot{ halfKey, 0 }),
            plan.effects[1].passes[0].output->slot);
    EXPECT_EQ(8u, plan.snapshots[0].lastUse);
}

TEST(ScratchPlan, outputNeverAliasesAnInputOfAWideGraph)
{
    Effect effect = avg::makeEffect({
        regionPass({ PassInput::backdrop() }, Divisor::Two,
                ase::FORMAT_RGBA16F),
        withSource(regionPass({ PassInput::backdrop() }, Divisor::Two,
                    ase::FORMAT_RGBA16F), "other"),
        regionPass({ PassInput::pass(0), PassInput::pass(1) }, Divisor::Two,
                ase::FORMAT_RGBA16F),
        finalPass({ PassInput::pass(2) })
    });
    PassGraph graph(screen);
    graph.addEffect(effect, graph.addSnapshot(), shape);

    ScratchPlan plan = avg::planScratch(graph);

    auto const& passes = plan.effects[0].passes;
    EXPECT_EQ(3u, plan.getPoolSize(halfKey));
    EXPECT_EQ((ScratchSlot{ halfKey, 2 }), passes[2].output->slot);
    EXPECT_EQ((ScratchSlot{ halfKey, 0 }), passes[2].inputs[0]);
    EXPECT_EQ((ScratchSlot{ halfKey, 1 }), passes[2].inputs[1]);
}

TEST(ScratchPlan, nestedScopesHoldTwoSnapshots)
{
    Effect tint = avg::makeEffect({ finalPass({ PassInput::backdrop() }) });
    PassGraph graph(screen);
    size_t const outer = graph.addSnapshot();
    graph.addEffect(tint, outer, shape);
    size_t const inner = graph.addSnapshot();
    graph.addEffect(tint, inner, shape);
    graph.addEffect(tint, outer, shape);

    ScratchPlan plan = avg::planScratch(graph);

    EXPECT_EQ(2u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ((ScratchSlot{ snapshotKey, 0 }), plan.snapshots[outer].slot);
    EXPECT_EQ((ScratchSlot{ snapshotKey, 1 }), plan.snapshots[inner].slot);
    EXPECT_EQ(4u, plan.snapshots[outer].lastUse);
    EXPECT_EQ(3u, plan.snapshots[inner].lastUse);
    EXPECT_EQ(plan.snapshots[inner].slot, plan.effects[1].passes[0].inputs[0]);
}

TEST(ScratchPlan, unreadSnapshotIsReleasedAtOnce)
{
    PassGraph graph(screen);
    graph.addSnapshot();
    graph.addSnapshot();

    ScratchPlan plan = avg::planScratch(graph);

    EXPECT_EQ(1u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ(plan.snapshots[0].definedAt, plan.snapshots[0].lastUse);
    EXPECT_EQ(plan.snapshots[0].slot, plan.snapshots[1].slot);
}

TEST(ScratchPlan, poolsAreCountedPerKey)
{
    Effect effect = avg::makeEffect({
        regionPass({ PassInput::backdrop() }, Divisor::Two,
                ase::FORMAT_RGBA16F),
        regionPass({ PassInput::pass(0) }, Divisor::Two, ase::FORMAT_R8),
        regionPass({ PassInput::pass(1) }, Divisor::One, ase::FORMAT_SRGBA),
        finalPass({ PassInput::pass(2), PassInput::backdrop() })
    });
    PassGraph graph(screen);
    graph.addEffect(effect, graph.addSnapshot(), shape);

    ScratchPlan plan = avg::planScratch(graph);

    EXPECT_EQ(3u, plan.pools.size());
    EXPECT_EQ(1u, plan.getPoolSize(halfKey));
    EXPECT_EQ(1u, plan.getPoolSize(key(Divisor::Two, ase::FORMAT_R8)));
    // The snapshot is still live when pass 2 writes at full size.
    EXPECT_EQ(2u, plan.getPoolSize(snapshotKey));
    EXPECT_EQ(0u, plan.getPoolSize(key(Divisor::Eight, ase::FORMAT_R8)));
}

TEST(PassGraph, rejectsAnUnknownSnapshot)
{
    Effect tint = avg::makeEffect({ finalPass({ PassInput::backdrop() }) });
    PassGraph graph(screen);

    EXPECT_THROW(graph.addEffect(tint, 0, shape), std::invalid_argument);
}

TEST(Pass, equalPassesShareShaderIdentityAndBytes)
{
    avg::PassShader const shader = avg::makePassShader(1, {
            { "tint", ase::UniformType::Float4 },
            }, "void main() {}\n");

    Pass pass;
    pass.shader = shader;
    pass.inputs = { PassInput::backdrop() };
    pass.geometry = Geometry::Shape;
    pass.parameters = ase::UniformValues(shader.interface, "ParamBlock");
    pass.parameters->set("tint", ase::Vector4f(1.0f, 0.0f, 0.0f, 0.5f));

    Pass const same = pass;
    EXPECT_EQ(pass, same);

    Pass otherParameters = pass;
    otherParameters.parameters->set("tint",
            ase::Vector4f(1.0f, 0.0f, 0.0f, 0.25f));
    EXPECT_NE(pass, otherParameters);

    Pass otherInputs = pass;
    otherInputs.inputs = { PassInput::pass(0) };
    EXPECT_NE(pass, otherInputs);

    Pass otherOutput = pass;
    otherOutput.output = PassOutput{ Divisor::Two, ase::FORMAT_RGBA16F };
    EXPECT_NE(pass, otherOutput);

    Pass otherPadding = pass;
    otherPadding.padding = 1;
    EXPECT_NE(pass, otherPadding);
}

TEST(Pass, theSameSourceMadeTwiceIsAnotherShader)
{
    avg::PassShader const first = avg::makePassShader(1, {},
            "void main() {}\n");
    avg::PassShader const second = avg::makePassShader(1, {},
            "void main() {}\n");
    ASSERT_EQ(first.fragmentSource, second.fragmentSource);

    Pass a = finalPass({ PassInput::backdrop() });
    a.shader = first;
    Pass b = a;
    b.shader = second;

    EXPECT_NE(first.id, second.id);
    EXPECT_NE(a, b);
}

TEST(Effect, comparesPassByPass)
{
    Effect blur = avg::makeEffect(blurPasses());
    Effect copy = blur;

    EXPECT_EQ(blur, copy);
    EXPECT_EQ(blur, avg::makeEffect(blurPasses()));

    auto passes = blurPasses();
    passes[1].padding = 2;
    EXPECT_NE(blur, avg::makeEffect(passes));
}

TEST(Effect, withoutARecipeInterpolatesBySwitching)
{
    Effect blur = avg::makeEffect(blurPasses());
    Effect tint = avg::makeEffect({ finalPass({ PassInput::backdrop() }) });

    EXPECT_FALSE(avg::isSameKind(blur, blur));
    EXPECT_EQ(tint, avg::lerp(blur, tint, 0.0f));
    EXPECT_EQ(tint, avg::lerp(blur, tint, 0.5f));
}

TEST(Effect, aMovedFromEffectIsStillTheEffect)
{
    Effect blur = avg::makeEffect(blurPasses());
    Effect tint = avg::makeEffect({ finalPass({ PassInput::backdrop() }) });

    Effect moved = std::move(blur);
    Effect assigned = tint;
    assigned = std::move(tint);

    EXPECT_EQ(moved, blur);
    EXPECT_EQ(4u, blur.getPasses().size());
    EXPECT_FALSE(avg::isSameKind(blur, moved));
    EXPECT_EQ(assigned, avg::lerp(blur, tint, 0.5f));
}

TEST(Pass, withoutAShaderIdentityTheSourceDecides)
{
    Pass a = finalPass({ PassInput::backdrop() });
    a.shader.fragmentSource = "void main() {}\n";
    Pass b = a;
    EXPECT_EQ(a, b);

    b.shader.fragmentSource = "void main() { discard; }\n";
    EXPECT_NE(a, b);
}

namespace
{
    Effect tintedCopy(float strength)
    {
        static avg::PassShader const shader = avg::makePassShader(1, {
                { "tint", ase::UniformType::Float4 },
                }, "void main() {}\n");

        Pass copy = regionPass({ PassInput::backdrop() }, Divisor::Two,
                ase::FORMAT_RGBA16F);
        copy.shader = shader;
        copy.parameters = ase::UniformValues(shader.interface, "ParamBlock");
        copy.parameters->set("tint",
                ase::Vector4f(1.0f, 0.0f, 0.0f, strength));

        return avg::makeEffect({ copy, finalPass({ PassInput::pass(0) }) });
    }
} // anonymous namespace

TEST(PassGraph, equalChainsOverOneSnapshotMerge)
{
    Effect blur = avg::makeEffect(blurPasses());
    PixelRect const other = rect(20, 200, 60, 60);

    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    graph.addEffect(blur, snapshot, shape);
    graph.addEffect(blur, snapshot, other);

    EffectRegions const first = avg::deriveRegions(blur, shape, screen);
    EffectRegions const second = avg::deriveRegions(blur, other, screen);

    auto const& nodes = graph.getNodes();
    ASSERT_EQ(5u, nodes.size());
    ASSERT_EQ(3u, graph.getItems().size());
    EXPECT_EQ((std::vector<size_t>{ 0, 1, 2, 4 }), graph.getItems()[2].nodes);

    for (size_t i = 0; i < 3; ++i)
    {
        EXPECT_EQ(first.passRegions[i].unionWith(second.passRegions[i]),
                nodes[i].region);
    }

    EXPECT_EQ(shape, nodes[3].region);
    EXPECT_EQ(other, nodes[4].region);
    EXPECT_EQ(first.backdropRegion.unionWith(second.backdropRegion),
            graph.getBackdropRegion(snapshot));
}

TEST(PassGraph, passesWithDifferentParametersDoNotMerge)
{
    Effect const half = tintedCopy(0.5f);
    Effect const alsoHalf = tintedCopy(0.5f);
    Effect const quarter = tintedCopy(0.25f);

    PassGraph same(screen);
    size_t const a = same.addSnapshot();
    same.addEffect(half, a, shape);
    same.addEffect(alsoHalf, a, shape);
    EXPECT_EQ(3u, same.getNodes().size());

    PassGraph differing(screen);
    size_t const b = differing.addSnapshot();
    differing.addEffect(half, b, shape);
    differing.addEffect(quarter, b, shape);
    EXPECT_EQ(4u, differing.getNodes().size());
}

TEST(PassGraph, equalPassesOverDifferentInputsDoNotMerge)
{
    auto chain = [](std::string const& copySource)
    {
        return avg::makeEffect({
            withSource(regionPass({ PassInput::backdrop() }, Divisor::Two,
                        ase::FORMAT_RGBA16F), copySource),
            regionPass({ PassInput::pass(0) }, Divisor::Two,
                    ase::FORMAT_RGBA16F, 4),
            finalPass({ PassInput::pass(1) })
        });
    };
    Effect first = chain("copy");
    Effect second = chain("other copy");

    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    graph.addEffect(first, snapshot, shape);
    graph.addEffect(second, snapshot, shape);

    ASSERT_EQ(first.getPasses()[1], second.getPasses()[1]);
    EXPECT_EQ(6u, graph.getNodes().size());
}

TEST(PassGraph, differentScopesNeverShare)
{
    Effect blur = avg::makeEffect(blurPasses());

    PassGraph graph(screen);
    graph.addEffect(blur, graph.addSnapshot(), shape);
    graph.addEffect(blur, graph.addSnapshot(), shape);

    EXPECT_EQ(8u, graph.getNodes().size());
}

TEST(ScratchPlan, aSharedValueLivesUntilItsLastReader)
{
    Effect blur = avg::makeEffect(blurPasses());

    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    graph.addEffect(blur, snapshot, shape);
    graph.addEffect(blur, snapshot, rect(20, 200, 60, 60));

    ScratchPlan plan = avg::planScratch(graph);

    ASSERT_EQ(2u, plan.effects.size());
    auto const& first = plan.effects[0].passes;
    auto const& second = plan.effects[1].passes;
    ASSERT_EQ(4u, first.size());
    ASSERT_EQ(1u, second.size());
    EXPECT_EQ(3u, second[0].pass);
    EXPECT_EQ(5u, second[0].step);

    EXPECT_EQ(first[2].output->slot, second[0].inputs[0]);
    EXPECT_EQ(5u, first[2].output->lastUse);
    EXPECT_EQ(5u, plan.snapshots[0].lastUse);
    EXPECT_EQ(2u, plan.getPoolSize(halfKey));
}

TEST(ElementPass, needsAnOutputRegionGeometryAndNoInputs)
{
    Pass pass = regionPass({}, Divisor::Two, ase::FORMAT_RGBA16F, 3);
    pass.shader.interface = samplers(2);

    avg::ElementPass element = avg::makeElementPass(pass);
    EXPECT_EQ(2u, element.getInputCount());
    EXPECT_EQ(pass, element.getPass());
    EXPECT_EQ(element, avg::makeElementPass(pass));

    Pass withoutOutput = pass;
    withoutOutput.output.reset();
    EXPECT_THROW(avg::makeElementPass(withoutOutput), std::invalid_argument);

    Pass withInputs = pass;
    withInputs.inputs = { PassInput::backdrop(), PassInput::backdrop() };
    EXPECT_THROW(avg::makeElementPass(withInputs), std::invalid_argument);

    Pass negative = pass;
    negative.padding = -1;
    EXPECT_THROW(avg::makeElementPass(negative), std::invalid_argument);
}

TEST(ElementPass, reachesItsPaddingUnlessGivenLessPerAxis)
{
    Pass pass = regionPass({}, Divisor::Two, ase::FORMAT_RGBA16F, 3);
    pass.shader.interface = samplers(1);

    EXPECT_EQ(avg::Vector2i(3, 3), avg::makeElementPass(pass).getReach());

    avg::ElementPass const horizontal = avg::makeElementPass(pass, nullptr,
            avg::Vector2i(3, 0));
    EXPECT_EQ(avg::Vector2i(3, 0), horizontal.getReach());
    EXPECT_NE(avg::makeElementPass(pass), horizontal);

    EXPECT_THROW(avg::makeElementPass(pass, nullptr, avg::Vector2i(4, 0)),
            std::invalid_argument);
    EXPECT_THROW(avg::makeElementPass(pass, nullptr, avg::Vector2i(0, -1)),
            std::invalid_argument);
}

TEST(PassGraph, anElementPassGrowsTheSourcesItReads)
{
    using Source = PassGraph::Source;

    // A blur at divisor two reading a sub-drawing, composited by a final
    // pass reading the blur and the backdrop.
    Pass blur = regionPass({ PassInput::pass(0) }, Divisor::Two,
            ase::FORMAT_RGBA16F, 4);
    Pass composite = finalPass({ PassInput::pass(0), PassInput::backdrop() },
            1);

    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    size_t const content = graph.addSubDrawing();
    size_t const blurred = graph.addElement(blur, snapshot,
            { Source{ Source::Kind::SubDrawing, content } }, PixelRect());
    graph.addElement(composite, snapshot,
            { Source{ Source::Kind::Element, blurred },
            Source{ Source::Kind::Backdrop, 0 } }, shape);

    auto const& nodes = graph.getNodes();
    ASSERT_EQ(3u, nodes.size());
    EXPECT_TRUE(nodes[0].subDrawing);
    EXPECT_EQ(shape, nodes[2].region);
    // The final region grown by 1, at divisor two.
    EXPECT_EQ(rect(49, 39, 102, 22), nodes[1].region);
    // The blur's region grown by 4, back at divisor one.
    EXPECT_EQ(rect(90, 70, 220, 60), nodes[0].region);
    EXPECT_EQ(shape.grown(1), graph.getBackdropRegion(snapshot));
}

TEST(PassGraph, subDrawingsNeverMergeButEqualElementPassesDo)
{
    using Source = PassGraph::Source;

    Pass copy = regionPass({ PassInput::backdrop() }, Divisor::Two,
            ase::FORMAT_RGBA16F);
    Pass draw = finalPass({ PassInput::pass(0) });

    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    size_t const first = graph.addSubDrawing();
    size_t const second = graph.addSubDrawing();
    EXPECT_EQ(2u, graph.getSubDrawingCount());

    size_t const a = graph.addElement(copy, snapshot,
            { Source{ Source::Kind::Backdrop, 0 } }, PixelRect());
    size_t const b = graph.addElement(copy, snapshot,
            { Source{ Source::Kind::Backdrop, 0 } }, PixelRect());
    graph.addElement(draw, snapshot, { Source{ Source::Kind::Element, a } },
            shape);
    graph.addElement(draw, snapshot, { Source{ Source::Kind::Element, b } },
            rect(0, 0, 10, 10));

    EXPECT_NE(first, second);
    // Two sub-drawings, one shared copy and two final passes.
    EXPECT_EQ(5u, graph.getNodes().size());
    EXPECT_EQ(rect(0, 0, 150, 60), graph.getNodes()[2].region);

    EXPECT_THROW(graph.addElement(draw, snapshot,
                { Source{ Source::Kind::SubDrawing, 2 } }, shape),
            std::invalid_argument);
}

TEST(PassGraph, equalElementPassesOverDifferentSubDrawingsDoNotMerge)
{
    using Source = PassGraph::Source;

    Pass copy = regionPass({ PassInput::pass(0) }, Divisor::Two,
            ase::FORMAT_RGBA16F);
    Pass draw = finalPass({ PassInput::pass(0) });

    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    size_t const first = graph.addSubDrawing();
    size_t const second = graph.addSubDrawing();

    size_t const a = graph.addElement(copy, snapshot,
            { Source{ Source::Kind::SubDrawing, first } }, PixelRect());
    size_t const b = graph.addElement(copy, snapshot,
            { Source{ Source::Kind::SubDrawing, second } }, PixelRect());
    size_t const c = graph.addElement(copy, snapshot,
            { Source{ Source::Kind::SubDrawing, first } }, PixelRect());
    graph.addElement(draw, snapshot, { Source{ Source::Kind::Element, a } },
            shape);
    graph.addElement(draw, snapshot, { Source{ Source::Kind::Element, b } },
            shape);
    graph.addElement(draw, snapshot, { Source{ Source::Kind::Element, c } },
            shape);

    // Two sub-drawings, a copy of each, the second copy of the first
    // merged, and three final passes.
    EXPECT_EQ(7u, graph.getNodes().size());
}

TEST(ScratchPlan, aSubDrawingLivesUntilItsReaderAndAcrossItsContent)
{
    using Source = PassGraph::Source;

    Effect blur = avg::makeEffect(blurPasses());
    Pass draw = finalPass({ PassInput::pass(0) });

    // The sub-drawing holds a blur over the snapshot; an element then
    // composites the sub-drawing.
    PassGraph graph(screen);
    size_t const snapshot = graph.addSnapshot();
    size_t const content = graph.addSubDrawing();
    graph.addEffect(blur, snapshot, shape);
    graph.addElement(draw, snapshot,
            { Source{ Source::Kind::SubDrawing, content } }, shape);

    ScratchPlan plan = avg::planScratch(graph);
    ASSERT_EQ(1u, plan.subDrawings.size());
    ASSERT_EQ(2u, plan.effects.size());

    avg::ValueAllocation const& sub = plan.subDrawings[0];
    EXPECT_EQ(1u, sub.definedAt);
    EXPECT_EQ(plan.effects[1].passes.at(0).step, sub.lastUse);
    EXPECT_EQ(ScratchKey{}, sub.slot.key);
    EXPECT_NE(plan.snapshots[0].slot, sub.slot);
    EXPECT_EQ(sub.slot, plan.effects[1].passes.at(0).inputs.at(0));
}

