#include <ase/buffer.h>
#include <ase/dummyplatform.h>
#include <ase/dummyrendercontext.h>
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

#include <stdexcept>
#include <string>

using namespace ase;

namespace
{
    ShaderInterface::Block layout(std::vector<ShaderInterface::MemberDecl> m)
    {
        ShaderInterface iface;
        iface.block("B", 0, BlockLayout::Std140, std::move(m));
        return iface.getBlocks().front();
    }

    Buffer bytes(std::string const& source)
    {
        return Buffer(source.data(), source.size());
    }

    ShaderInterface painterInterface()
    {
        ShaderInterface iface;
        iface.attribute("pos", 0, TypeFloat, 3)
            .attribute("color", 1, TypeFloat, 4)
            .block("MatrixBlock", 0, BlockLayout::Std140, {
                    { "worldViewProj", UniformType::Mat4 },
                })
            .sampler("atlas", 0);
        return iface;
    }

    struct Fixture
    {
        btl::RunLoop loop;
        Platform platform = makeDummyPlatform(loop);
        RenderContext context = platform.makeRenderContext();

        Program program(ShaderInterface iface)
        {
            Shader vs = context.makeShader(ShaderStage::Vertex,
                    ShaderLanguage::Glsl330, bytes("vertex"));
            Shader fs = context.makeShader(ShaderStage::Fragment,
                    ShaderLanguage::Glsl330, bytes("fragment"));
            return context.makeProgram(vs, fs, std::move(iface));
        }
    };

    // A dummy that compiles only one language, to exercise the check that
    // the dummy itself never fails.
    class Glsl330OnlyContext : public DummyRenderContext
    {
    public:
        Glsl330OnlyContext() : DummyRenderContext(nullptr) {}

        std::vector<ShaderLanguage> const& getShaderLanguages()
            const override
        {
            static std::vector<ShaderLanguage> const languages = {
                ShaderLanguage::Glsl330
            };
            return languages;
        }
    };
} // anonymous namespace

TEST(shaderInterface, std140Scalars)
{
    auto block = layout({
            { "a", UniformType::Float },
            { "b", UniformType::Int },
            { "c", UniformType::UInt },
            });

    EXPECT_EQ(0u, block.members[0].offset);
    EXPECT_EQ(4u, block.members[1].offset);
    EXPECT_EQ(8u, block.members[2].offset);
    EXPECT_EQ(16u, block.size);
}

TEST(shaderInterface, std140Vec2)
{
    auto block = layout({
            { "a", UniformType::Float },
            { "b", UniformType::Float2 },
            { "c", UniformType::Float },
            });

    EXPECT_EQ(0u, block.members[0].offset);
    EXPECT_EQ(8u, block.members[1].offset);
    EXPECT_EQ(16u, block.members[2].offset);
    EXPECT_EQ(32u, block.size);
}

TEST(shaderInterface, std140Vec3PadsToVec4)
{
    auto block = layout({
            { "a", UniformType::Float },
            { "b", UniformType::Float3 },
            { "c", UniformType::Float },
            { "d", UniformType::Float4 },
            });

    EXPECT_EQ(0u, block.members[0].offset);
    EXPECT_EQ(16u, block.members[1].offset);
    EXPECT_EQ(28u, block.members[2].offset);
    EXPECT_EQ(32u, block.members[3].offset);
    EXPECT_EQ(48u, block.size);
}

TEST(shaderInterface, std140Matrices)
{
    auto block = layout({
            { "a", UniformType::Float },
            { "m4", UniformType::Mat4 },
            { "m3", UniformType::Mat3 },
            { "m2", UniformType::Mat2 },
            });

    EXPECT_EQ(16u, block.members[1].offset);
    EXPECT_EQ(80u, block.members[2].offset);
    EXPECT_EQ(128u, block.members[3].offset);
    EXPECT_EQ(160u, block.size);
}

TEST(shaderInterface, std140ArraysStrideVec4)
{
    auto block = layout({
            { "a", UniformType::Float, 3 },
            { "b", UniformType::Float },
            { "c", UniformType::Float4, 2 },
            { "d", UniformType::Mat4, 2 },
            });

    EXPECT_EQ(0u, block.members[0].offset);
    EXPECT_EQ(16u, block.members[0].arrayStride);
    EXPECT_EQ(48u, block.members[1].offset);
    EXPECT_EQ(0u, block.members[1].arrayStride);
    EXPECT_EQ(64u, block.members[2].offset);
    EXPECT_EQ(16u, block.members[2].arrayStride);
    EXPECT_EQ(96u, block.members[3].offset);
    EXPECT_EQ(64u, block.members[3].arrayStride);
    EXPECT_EQ(224u, block.size);
}

TEST(shaderInterface, lookupsByName)
{
    auto iface = painterInterface();

    ASSERT_NE(nullptr, iface.findAttribute("color"));
    EXPECT_EQ(1, iface.findAttribute("color")->location);
    ASSERT_NE(nullptr, iface.findBlock("MatrixBlock"));
    ASSERT_NE(nullptr, iface.findBlock("MatrixBlock")->findMember(
                "worldViewProj"));
    EXPECT_EQ(64u, iface.findBlock("MatrixBlock")->size);
    ASSERT_NE(nullptr, iface.findSampler("atlas"));
    EXPECT_EQ(nullptr, iface.findAttribute("uv"));
    EXPECT_EQ(nullptr, iface.findBlock("Missing"));
    EXPECT_EQ(nullptr, iface.findSampler("missing"));
}

TEST(shaderInterface, duplicatesAreErrors)
{
    ShaderInterface iface = painterInterface();

    EXPECT_THROW(iface.attribute("pos", 5, TypeFloat, 3),
            std::invalid_argument);
    EXPECT_THROW(iface.attribute("other", 0, TypeFloat, 3),
            std::invalid_argument);
    EXPECT_THROW(iface.attribute("neg", -1, TypeFloat, 3),
            std::invalid_argument);
    EXPECT_THROW(iface.attribute("wide", 7, TypeFloat, 5),
            std::invalid_argument);
    EXPECT_THROW(iface.block("MatrixBlock", 1, BlockLayout::Std140, {}),
            std::invalid_argument);
    EXPECT_THROW(iface.block("Other", 0, BlockLayout::Std140, {}),
            std::invalid_argument);
    EXPECT_THROW(iface.block("Members", 1, BlockLayout::Std140, {
                { "a", UniformType::Float },
                { "a", UniformType::Float },
                }), std::invalid_argument);
    EXPECT_THROW(iface.block("Array", 1, BlockLayout::Std140, {
                { "a", UniformType::Float, -1 },
                }), std::invalid_argument);
    EXPECT_THROW(iface.sampler("atlas", 3), std::invalid_argument);
    EXPECT_THROW(iface.sampler("other", 0), std::invalid_argument);

    EXPECT_EQ(painterInterface(), iface);
}

TEST(shaderInterface, mismatchesAreFound)
{
    auto declared = painterInterface();
    EXPECT_TRUE(findMismatches(declared, declared).empty());

    ShaderInterface compiled;
    compiled.attribute("pos", 0, TypeFloat, 4)
        .attribute("uv", 2, TypeFloat, 2)
        .block("MatrixBlock", 1, BlockLayout::Std140, {
                { "worldViewProj", UniformType::Mat3 },
                { "extra", UniformType::Float },
            })
        .sampler("atlas", 1)
        .sampler("mask", 2);

    auto mismatches = findMismatches(declared, compiled);
    EXPECT_EQ(8u, mismatches.size());
}

TEST(shaderInterface, compiledBlockMayBeLarger)
{
    ShaderInterface declared;
    declared.block("B", 0, BlockLayout::Std140, {
            { "a", UniformType::Float },
            });

    ShaderInterface::Block padded = declared.getBlocks().front();
    padded.size = 256;
    ShaderInterface compiled;
    compiled.block(padded);

    EXPECT_TRUE(findMismatches(declared, compiled).empty());
    EXPECT_FALSE(findMismatches(compiled, declared).empty());
}

TEST(shaderInterface, dummyResolvesVertexSpecFromDeclaration)
{
    Fixture f;
    Program program = f.program(painterInterface());

    EXPECT_EQ(painterInterface(), program.getInterface());
    EXPECT_FALSE(program.reflect().has_value());
    EXPECT_EQ(1, program.getAttribLocation("color"));
    EXPECT_EQ(-1, program.getAttribLocation("uv"));

    NamedVertexSpec named;
    named.add("color", 4, TypeFloat, false)
        .add("pos", 3, TypeFloat, false);

    Pipeline pipeline = f.context.makePipeline(program,
            VertexSpec(program, named), PipelineDesc{ FORMAT_SRGBA });
    auto const& specs = pipeline.getVertexSpec().getSpecs();

    ASSERT_EQ(2u, specs.size());
    EXPECT_EQ(0, specs[0].attribLoc);
    EXPECT_EQ(3, specs[0].size);
    EXPECT_EQ(16u, specs[0].pointer);
    EXPECT_EQ(1, specs[1].attribLoc);
    EXPECT_EQ(4, specs[1].size);
    EXPECT_EQ(0u, specs[1].pointer);
    EXPECT_EQ(28u, pipeline.getVertexSpec().getStride());
}

TEST(shaderInterface, undeclaredAttributeIsAnError)
{
    Fixture f;
    Program program = f.program(painterInterface());

    NamedVertexSpec named;
    named.add("colour", 4, TypeFloat, false);

    EXPECT_THROW(VertexSpec(program, named), std::invalid_argument);
}

TEST(shaderInterface, wrongStageIsAnError)
{
    Fixture f;
    Shader vs = f.context.makeShader(ShaderStage::Vertex,
            ShaderLanguage::Glsl330, bytes("vertex"));
    Shader fs = f.context.makeShader(ShaderStage::Fragment,
            ShaderLanguage::Glsl330, bytes("fragment"));

    EXPECT_EQ(ShaderStage::Vertex, vs.getStage());
    EXPECT_THROW(f.context.makeProgram(fs, fs, ShaderInterface()),
            std::invalid_argument);
    EXPECT_THROW(f.context.makeProgram(vs, vs, ShaderInterface()),
            std::invalid_argument);
}

TEST(shaderInterface, dummyAcceptsEveryLanguage)
{
    Fixture f;
    auto const& languages = f.context.getShaderLanguages();

    EXPECT_EQ(6u, languages.size());
    EXPECT_EQ(ShaderLanguage::Glsl330, languages.front());
    for (auto language : languages)
    {
        EXPECT_NO_THROW(f.context.makeShader(ShaderStage::Vertex, language,
                    bytes("source")));
    }
}

TEST(shaderInterface, unlistedLanguageThrows)
{
    RenderContext context(std::make_shared<Glsl330OnlyContext>());

    EXPECT_NO_THROW(context.makeShader(ShaderStage::Vertex,
                ShaderLanguage::Glsl330, bytes("source")));

    try
    {
        context.makeShader(ShaderStage::Vertex, ShaderLanguage::Wgsl,
                bytes("source"));
        FAIL() << "expected UnsupportedShaderLanguage";
    }
    catch (UnsupportedShaderLanguage const& e)
    {
        EXPECT_EQ(ShaderLanguage::Wgsl, e.getLanguage());
        EXPECT_NE(std::string::npos, std::string(e.what()).find("wgsl"));
    }
}
