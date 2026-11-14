#include "shadersource.h"

#include <ase/shader.h>
#include <ase/shaderstage.h>

#include <algorithm>

namespace avg
{

namespace
{
    constexpr ase::ShaderLanguage preferredLanguages[] = {
        ase::ShaderLanguage::SpirV,
        ase::ShaderLanguage::Glsl330
    };

    bool hasBytesFor(ase::ShaderLanguage language)
    {
        return language == ase::ShaderLanguage::Glsl330;
    }

    ase::Buffer bytesOf(std::string const& text)
    {
        return ase::Buffer(text.data(), text.size());
    }
} // anonymous namespace

ase::ShaderLanguage selectShaderLanguage(
        std::vector<ase::ShaderLanguage> const& supported)
{
    for (ase::ShaderLanguage language : preferredLanguages)
    {
        if (hasBytesFor(language) && std::find(supported.begin(),
                    supported.end(), language) != supported.end())
        {
            return language;
        }
    }

    throw ase::UnsupportedShaderLanguage(ase::ShaderLanguage::Glsl330);
}

ShaderBytes shaderBytes(ase::RenderContext const& context,
        ShaderSource const& source)
{
    ase::ShaderLanguage const language = selectShaderLanguage(
            context.getShaderLanguages());

    return ShaderBytes{ language, bytesOf(source.vertex),
        bytesOf(source.fragment), source.iface };
}

ase::Program makeProgram(ase::RenderContext& context,
        ShaderSource const& source)
{
    ShaderBytes bytes = shaderBytes(context, source);

    ase::Shader vs = context.makeShader(ase::ShaderStage::Vertex,
            bytes.language, bytes.vertex);
    ase::Shader fs = context.makeShader(ase::ShaderStage::Fragment,
            bytes.language, bytes.fragment);

    return context.makeProgram(std::move(vs), std::move(fs),
            std::move(bytes.iface));
}

} // namespace avg
