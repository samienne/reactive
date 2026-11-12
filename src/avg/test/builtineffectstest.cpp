#include <avg/builtineffects.h>
#include <avg/effect.h>
#include <avg/effectplan.h>
#include <avg/effectshader.h>

#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using avg::Color;
using avg::Divisor;
using avg::Effect;
using avg::EffectRegions;
using avg::FrostedGlassOptions;
using avg::Geometry;
using avg::Pass;
using avg::PassInput;
using avg::PassShader;
using avg::PixelRect;
using avg::ScratchKey;
using avg::ScratchPlan;
using avg::ScratchTimeline;
using avg::Vector2i;

namespace
{
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

    bool contains(std::string const& text, std::string const& needle)
    {
        return text.find(needle) != std::string::npos;
    }

    // The declaration of a block as it stands in a shader source, from
    // its layout qualifier to the end of its instance name.
    std::string blockDeclaration(std::string const& source,
            std::string const& block)
    {
        size_t begin = source.find("layout(std140) uniform " + block);
        if (begin == std::string::npos)
            return std::string();

        size_t end = source.find(";", source.find("}", begin));
        return source.substr(begin, end - begin);
    }

    // Reads the four floats of a Float4 member of a pass's parameters.
    std::vector<float> readFloat4(Pass const& pass, std::string const& name)
    {
        EXPECT_TRUE(pass.parameters.has_value());
        if (!pass.parameters)
            return std::vector<float>();

        ase::ShaderInterface::Member const* member =
            pass.parameters->getBlock().findMember(name);
        EXPECT_NE(nullptr, member);
        if (!member || member->type != ase::UniformType::Float4)
            return std::vector<float>();

        std::vector<float> values(4);
        std::memcpy(values.data(), static_cast<char const*>(
                    pass.parameters->data()) + member->offset,
                4 * sizeof(float));
        return values;
    }

    void expectShaderDeclaresWhatItUses(Pass const& pass)
    {
        std::string const& source = pass.shader.fragmentSource;
        ase::ShaderInterface const& iface = pass.shader.interface;

        EXPECT_EQ(0u, source.find("#version 330\n"));
        EXPECT_FALSE(contains(source, "layout(binding"));
        EXPECT_FALSE(contains(source, "420pack"));

        ASSERT_EQ(pass.inputs.size(), iface.getSamplers().size());
        for (size_t i = 0; i < iface.getSamplers().size(); ++i)
        {
            ase::ShaderInterface::Sampler const& sampler =
                iface.getSamplers()[i];
            EXPECT_EQ(static_cast<int>(i), sampler.unit);
            EXPECT_EQ(avg::passInputName(i), sampler.name);
            EXPECT_TRUE(contains(source, "uniform sampler2D "
                        + sampler.name + ";"));
        }

        ASSERT_EQ(1u, iface.getAttributes().size());
        EXPECT_EQ("pos", iface.getAttributes()[0].name);
        EXPECT_EQ(0, iface.getAttributes()[0].location);
        EXPECT_EQ(2, iface.getAttributes()[0].size);

        ASSERT_NE(nullptr, iface.findBlock("PassBlock"));
        EXPECT_EQ(0, iface.findBlock("PassBlock")->binding);
        EXPECT_EQ(blockDeclaration(avg::passVertexSource(), "PassBlock"),
                blockDeclaration(source, "PassBlock"));

        for (ase::ShaderInterface::Block const& block : iface.getBlocks())
        {
            EXPECT_TRUE(contains(source, "uniform " + block.name));
            for (ase::ShaderInterface::Member const& member : block.members)
                EXPECT_TRUE(contains(source, " " + member.name + ";"))
                    << block.name << "." << member.name;
        }

        if (pass.parameters)
        {
            ase::ShaderInterface::Block const* declared =
                iface.findBlock(pass.parameters->getBlock().name);
            ASSERT_NE(nullptr, declared);
            EXPECT_EQ(*declared, pass.parameters->getBlock());
            EXPECT_EQ(1, declared->binding);
        }
        else
            EXPECT_EQ(nullptr, iface.findBlock("ParamBlock"));
    }

    void expectDivisor(Effect const& effect, Divisor divisor)
    {
        std::vector<Pass> const& passes = effect.getPasses();
        for (size_t i = 0; i + 1 < passes.size(); ++i)
        {
            ASSERT_TRUE(passes[i].output.has_value());
            EXPECT_EQ(divisor, passes[i].output->divisor) << "pass " << i;
            EXPECT_EQ(ase::FORMAT_RGBA16F, passes[i].output->format)
                << "pass " << i;
        }
    }

    std::vector<int> paddings(Effect const& effect)
    {
        std::vector<int> result;
        for (Pass const& pass : effect.getPasses())
            result.push_back(pass.padding);
        return result;
    }

    FrostedGlassOptions glassWithRadius(float radius)
    {
        FrostedGlassOptions options;
        options.radius = radius;
        return options;
    }
} // anonymous namespace

TEST(PassShader, preludeDeclaresTheInterface)
{
    std::vector<avg::PassParameter> parameters = {
        { "tint", ase::UniformType::Float4 },
        { "weights", ase::UniformType::Float, 3 },
    };

    std::string prelude = avg::passFragmentPrelude(2, parameters);
    ase::ShaderInterface iface = avg::passInterface(2, parameters);

    EXPECT_EQ(0u, prelude.find("#version 330\n"));
    EXPECT_TRUE(contains(prelude, "uniform sampler2D input0;"));
    EXPECT_TRUE(contains(prelude, "uniform sampler2D input1;"));
    EXPECT_FALSE(contains(prelude, "input2"));
    EXPECT_TRUE(contains(prelude, "    vec4 tint;\n"));
    EXPECT_TRUE(contains(prelude, "    float weights[3];\n"));
    EXPECT_TRUE(contains(prelude, "} param;"));
    EXPECT_TRUE(contains(prelude, "} pass;"));
    EXPECT_TRUE(contains(prelude, "out vec4 result;"));
    EXPECT_TRUE(contains(prelude, "vec2 screenPos()"));
    EXPECT_TRUE(contains(prelude, "vec2 inputUv(vec4 texel)"));

    ASSERT_EQ(2u, iface.getSamplers().size());
    EXPECT_EQ("input1", iface.getSamplers()[1].name);
    EXPECT_EQ(1, iface.getSamplers()[1].unit);

    ase::ShaderInterface::Block const* params = iface.findBlock("ParamBlock");
    ASSERT_NE(nullptr, params);
    EXPECT_EQ(1, params->binding);
    ASSERT_EQ(2u, params->members.size());
    EXPECT_EQ(3, params->members[1].arraySize);

    ase::ShaderInterface::Block const* block = iface.findBlock("PassBlock");
    ASSERT_NE(nullptr, block);
    EXPECT_EQ(7u, block->members.size());
    EXPECT_NE(nullptr, block->findMember("targetSize"));
    EXPECT_NE(nullptr, block->findMember("targetDivisor"));
    EXPECT_NE(nullptr, block->findMember("region"));
    EXPECT_NE(nullptr, block->findMember(avg::passTexelName(3)));
}

TEST(PassShader, preludeWithoutParametersHasNoParamBlock)
{
    std::string prelude = avg::passFragmentPrelude(1, {});
    ase::ShaderInterface iface = avg::passInterface(1, {});

    EXPECT_FALSE(contains(prelude, "ParamBlock"));
    EXPECT_EQ(nullptr, iface.findBlock("ParamBlock"));
    EXPECT_EQ(1u, iface.getBlocks().size());
}

TEST(PassShader, vertexShaderDeclaresTheSameBlock)
{
    std::string const& vertex = avg::passVertexSource();

    EXPECT_EQ(0u, vertex.find("#version 330\n"));
    EXPECT_TRUE(contains(vertex, "in vec2 pos;"));
    EXPECT_FALSE(contains(vertex, "layout(binding"));
    EXPECT_FALSE(contains(vertex, "420pack"));

    std::string declaration = blockDeclaration(vertex, "PassBlock");
    EXPECT_FALSE(declaration.empty());
    EXPECT_EQ(declaration,
            blockDeclaration(avg::passFragmentPrelude(3, {}), "PassBlock"));
}

TEST(PassShader, rejectsTooManyInputs)
{
    EXPECT_NO_THROW(avg::passInterface(avg::maxPassInputs, {}));
    EXPECT_THROW(avg::passInterface(avg::maxPassInputs + 1, {}),
            std::invalid_argument);
    EXPECT_THROW(avg::passFragmentPrelude(avg::maxPassInputs + 1, {}),
            std::invalid_argument);
}

TEST(PassShader, makePassShaderAppendsTheBody)
{
    PassShader shader = avg::makePassShader(1, {}, "void main() {}\n");

    EXPECT_EQ(avg::passFragmentPrelude(1, {}) + "void main() {}\n",
            shader.fragmentSource);
    EXPECT_EQ(avg::passInterface(1, {}), shader.interface);
}

TEST(BuiltinEffects, frostedGlassIsAValidChainEndingInTheShape)
{
    Effect glass = avg::frostedGlass();
    std::vector<Pass> const& passes = glass.getPasses();

    EXPECT_TRUE(avg::validate(passes).empty());
    ASSERT_EQ(4u, passes.size());

    EXPECT_EQ(std::vector<PassInput>{ PassInput::backdrop() },
            passes[0].inputs);
    EXPECT_EQ(std::vector<PassInput>{ PassInput::pass(0) }, passes[1].inputs);
    EXPECT_EQ(std::vector<PassInput>{ PassInput::pass(1) }, passes[2].inputs);
    EXPECT_EQ(std::vector<PassInput>{ PassInput::pass(2) }, passes[3].inputs);

    for (size_t i = 0; i < 3; ++i)
        EXPECT_EQ(Geometry::Region, passes[i].geometry);
    EXPECT_EQ(Geometry::Shape, passes[3].geometry);
    EXPECT_FALSE(passes[3].output.has_value());
    EXPECT_EQ(&passes[3], &glass.getFinalPass());
}

TEST(BuiltinEffects, frostedGlassShadersDeclareWhatTheyUse)
{
    Effect glass = avg::frostedGlass();
    for (Pass const& pass : glass.getPasses())
        expectShaderDeclaresWhatItUses(pass);

    EXPECT_FALSE(glass.getPasses()[0].parameters.has_value());
    EXPECT_TRUE(glass.getPasses()[1].parameters.has_value());
    EXPECT_TRUE(glass.getPasses()[2].parameters.has_value());
    EXPECT_TRUE(glass.getPasses()[3].parameters.has_value());
}

TEST(BuiltinEffects, frostedGlassDivisorFollowsTheRadius)
{
    EXPECT_EQ(Divisor::Two, avg::frostedGlassDivisor(0.0f));
    EXPECT_EQ(Divisor::Two, avg::frostedGlassDivisor(32.0f));
    EXPECT_EQ(Divisor::Four, avg::frostedGlassDivisor(32.5f));
    EXPECT_EQ(Divisor::Four, avg::frostedGlassDivisor(64.0f));
    EXPECT_EQ(Divisor::Eight, avg::frostedGlassDivisor(64.5f));
    EXPECT_EQ(Divisor::Eight, avg::frostedGlassDivisor(1000.0f));

    expectDivisor(avg::frostedGlass(glassWithRadius(24.0f)), Divisor::Two);
    expectDivisor(avg::frostedGlass(glassWithRadius(40.0f)), Divisor::Four);
    expectDivisor(avg::frostedGlass(glassWithRadius(100.0f)),
            Divisor::Eight);
}

TEST(BuiltinEffects, frostedGlassPadsBlursByTheirTapRadius)
{
    // Radius 24 at half size is 12 texels; the final pass pads by half a
    // blurred texel for bilinear upsampling.
    EXPECT_EQ((std::vector<int>{ 0, 12, 12, 1 }),
            paddings(avg::frostedGlass(glassWithRadius(24.0f))));

    // Radius 38 at quarter size is 9.5 texels, rounded up to 10.
    EXPECT_EQ((std::vector<int>{ 0, 10, 10, 2 }),
            paddings(avg::frostedGlass(glassWithRadius(38.0f))));

    // Radius 100 at eighth size is 12.5 texels.
    EXPECT_EQ((std::vector<int>{ 0, 13, 13, 4 }),
            paddings(avg::frostedGlass(glassWithRadius(100.0f))));

    EXPECT_EQ((std::vector<int>{ 0, 0, 0, 1 }),
            paddings(avg::frostedGlass(glassWithRadius(0.0f))));
}

TEST(BuiltinEffects, frostedGlassBlurParametersFollowTheRadius)
{
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));

    std::vector<float> horizontal = readFloat4(glass.getPasses()[1], "blur");
    std::vector<float> vertical = readFloat4(glass.getPasses()[2], "blur");

    EXPECT_EQ((std::vector<float>{ 1.0f, 0.0f, 12.0f, 6.0f }), horizontal);
    EXPECT_EQ((std::vector<float>{ 0.0f, 1.0f, 12.0f, 6.0f }), vertical);

    // A radius of zero still gets a positive sigma.
    Effect sharp = avg::frostedGlass(glassWithRadius(0.0f));
    std::vector<float> none = readFloat4(sharp.getPasses()[1], "blur");
    EXPECT_EQ(0.0f, none[2]);
    EXPECT_GT(none[3], 0.0f);
}

TEST(BuiltinEffects, frostedGlassFinalPassCarriesTheLook)
{
    FrostedGlassOptions options;
    options.tint = Color(0.2f, 0.4f, 0.6f, 1.0f);
    options.tintStrength = 0.3f;
    options.saturation = 1.5f;
    options.highlight = Color(1.0f, 0.9f, 0.8f, 1.0f);
    options.highlightStrength = 0.25f;
    options.highlightWidth = 2.0f;

    Effect glass = avg::frostedGlass(options);
    Pass const& final = glass.getFinalPass();

    EXPECT_EQ((std::vector<float>{ 0.2f, 0.4f, 0.6f, 0.3f }),
            readFloat4(final, "tint"));
    EXPECT_EQ((std::vector<float>{ 1.0f, 0.9f, 0.8f, 0.25f }),
            readFloat4(final, "highlight"));
    EXPECT_EQ((std::vector<float>{ 1.5f, 2.0f, 0.0f, 0.0f }),
            readFloat4(final, "adjust"));
}

TEST(BuiltinEffects, frostedGlassRegionsGrowByEachPadding)
{
    Effect glass = avg::frostedGlass(glassWithRadius(24.0f));

    EffectRegions regions = avg::deriveRegions(glass,
            rect(100, 80, 200, 40), Vector2i(400, 300));

    ASSERT_EQ(4u, regions.passRegions.size());
    EXPECT_EQ(rect(100, 80, 200, 40), regions.passRegions[3]);
    EXPECT_EQ(rect(49, 39, 102, 22), regions.passRegions[2]);
    EXPECT_EQ(rect(37, 27, 126, 46), regions.passRegions[1]);
    EXPECT_EQ(rect(25, 15, 150, 70), regions.passRegions[0]);
    EXPECT_EQ(rect(50, 30, 300, 140), regions.backdropRegion);
}

TEST(BuiltinEffects, twoGlassesInOneScopeShareTwoBlurTextures)
{
    Effect first = avg::frostedGlass(glassWithRadius(24.0f));
    Effect second = avg::frostedGlass(glassWithRadius(16.0f));

    ScratchTimeline timeline;
    size_t snapshot = timeline.addSnapshot();
    timeline.addEffect(first, snapshot);
    timeline.addEffect(second, snapshot);

    ScratchPlan plan = avg::planScratch(timeline);

    EXPECT_EQ(1u, plan.getPoolSize(key(Divisor::One, ase::FORMAT_SRGBA)));
    EXPECT_EQ(2u, plan.getPoolSize(key(Divisor::Two, ase::FORMAT_RGBA16F)));
    EXPECT_EQ(0u, plan.getPoolSize(key(Divisor::Four, ase::FORMAT_RGBA16F)));

    // The snapshot is read only by the two copy passes, so it is released
    // once the second effect's first pass has run.
    ASSERT_EQ(1u, plan.snapshots.size());
    ASSERT_EQ(2u, plan.effects.size());
    EXPECT_EQ(plan.effects[1].passes[0].step, plan.snapshots[0].lastUse);
}

TEST(BuiltinEffects, frostedGlassRejectsBadOptions)
{
    EXPECT_THROW(avg::frostedGlass(glassWithRadius(-1.0f)),
            std::invalid_argument);

    FrostedGlassOptions strong;
    strong.tintStrength = 1.5f;
    EXPECT_THROW(avg::frostedGlass(strong), std::invalid_argument);

    FrostedGlassOptions negative;
    negative.highlightStrength = -0.1f;
    EXPECT_THROW(avg::frostedGlass(negative), std::invalid_argument);

    FrostedGlassOptions desaturated;
    desaturated.saturation = -1.0f;
    EXPECT_THROW(avg::frostedGlass(desaturated), std::invalid_argument);

    FrostedGlassOptions thin;
    thin.highlightWidth = -1.0f;
    EXPECT_THROW(avg::frostedGlass(thin), std::invalid_argument);
}

TEST(BuiltinEffects, tintIsOneShapePassReadingTheBackdrop)
{
    Effect dim = avg::tint(Color(0.0f, 0.0f, 0.0f, 0.5f), 0.5f);
    std::vector<Pass> const& passes = dim.getPasses();

    EXPECT_TRUE(avg::validate(passes).empty());
    ASSERT_EQ(1u, passes.size());
    EXPECT_EQ(Geometry::Shape, passes[0].geometry);
    EXPECT_EQ(std::vector<PassInput>{ PassInput::backdrop() },
            passes[0].inputs);
    EXPECT_EQ(0, passes[0].padding);
    expectShaderDeclaresWhatItUses(passes[0]);

    // The strength is scaled by the colour's alpha.
    EXPECT_EQ((std::vector<float>{ 0.0f, 0.0f, 0.0f, 0.25f }),
            readFloat4(passes[0], "tint"));

    EXPECT_THROW(avg::tint(Color(1.0f, 1.0f, 1.0f), 2.0f),
            std::invalid_argument);
}

TEST(BuiltinEffects, tintAloneNeedsOnlyTheSnapshot)
{
    Effect dim = avg::tint(Color(0.0f, 0.0f, 0.0f), 0.5f);

    ScratchTimeline timeline;
    size_t snapshot = timeline.addSnapshot();
    timeline.addEffect(dim, snapshot);

    ScratchPlan plan = avg::planScratch(timeline);

    ASSERT_EQ(1u, plan.pools.size());
    EXPECT_EQ(key(Divisor::One, ase::FORMAT_SRGBA), plan.pools[0].key);
    EXPECT_EQ(1u, plan.pools[0].count);
}
