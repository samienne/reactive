#include "shadersource.h"

#include <ase/dummyplatform.h>
#include <ase/platform.h>
#include <ase/rendercontext.h>
#include <ase/shaderlanguage.h>

#include <btl/runloop.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using ase::ShaderLanguage;

TEST(shadersource, glsl330IsSelectedWhenListed)
{
    EXPECT_EQ(ShaderLanguage::Glsl330,
            avg::selectShaderLanguage({ ShaderLanguage::Glsl330 }));
    EXPECT_EQ(ShaderLanguage::Glsl330,
            avg::selectShaderLanguage({ ShaderLanguage::Msl,
                ShaderLanguage::Glsl330 }));
}

TEST(shadersource, spirvWithoutBytesFallsThroughToGlsl330)
{
    EXPECT_EQ(ShaderLanguage::Glsl330,
            avg::selectShaderLanguage({ ShaderLanguage::SpirV,
                ShaderLanguage::Glsl330 }));
}

TEST(shadersource, noUsableLanguageThrows)
{
    EXPECT_THROW(avg::selectShaderLanguage({}),
            ase::UnsupportedShaderLanguage);
    EXPECT_THROW(avg::selectShaderLanguage({ ShaderLanguage::SpirV }),
            ase::UnsupportedShaderLanguage);

    try
    {
        avg::selectShaderLanguage({ ShaderLanguage::Msl });
        FAIL();
    }
    catch (ase::UnsupportedShaderLanguage const& e)
    {
        EXPECT_EQ(ShaderLanguage::Glsl330, e.getLanguage());
    }
}

TEST(shadersource, dummyGetsGlslPassedThrough)
{
    btl::RunLoop loop;
    ase::Platform platform = ase::makeDummyPlatform(loop);
    ase::RenderContext context = platform.makeRenderContext();

    std::string const vertex = "#version 330\nvoid main() {}\n";
    std::string const fragment = "#version 330\nout vec4 r;\n";
    ase::ShaderInterface iface;
    iface.attribute("pos", 0, ase::TypeFloat, 2);

    avg::ShaderBytes bytes = avg::shaderBytes(context,
            { vertex, fragment, iface });

    EXPECT_EQ(ShaderLanguage::Glsl330, bytes.language);
    ASSERT_EQ(vertex.size(), bytes.vertex.getSize());
    EXPECT_EQ(vertex, std::string(bytes.vertex.mapRead<char>(),
                bytes.vertex.getSize()));
    ASSERT_EQ(fragment.size(), bytes.fragment.getSize());
    EXPECT_EQ(fragment, std::string(bytes.fragment.mapRead<char>(),
                bytes.fragment.getSize()));
    ASSERT_EQ(1u, bytes.iface.getAttributes().size());

    ase::Program program = avg::makeProgram(context,
            { vertex, fragment, iface });
    (void)program;
}
