#include "shaderlanguage.h"

#include <string>

namespace ase
{

std::string_view toString(ShaderLanguage language)
{
    switch (language)
    {
        case ShaderLanguage::Glsl330:
            return "glsl330";
        case ShaderLanguage::GlslEs300:
            return "glsles300";
        case ShaderLanguage::Glsl450:
            return "glsl450";
        case ShaderLanguage::SpirV:
            return "spirv";
        case ShaderLanguage::Wgsl:
            return "wgsl";
        case ShaderLanguage::Msl:
            return "msl";
    }

    return "unknown";
}

UnsupportedShaderLanguage::UnsupportedShaderLanguage(ShaderLanguage language) :
    std::runtime_error("Unsupported shader language: "
            + std::string(toString(language))),
    language_(language)
{
}

/* Out of line so the type info lives in this library alone; a copy emitted
 * in another image is not matched by a catch on macOS. */
UnsupportedShaderLanguage::~UnsupportedShaderLanguage() = default;

ShaderLanguage UnsupportedShaderLanguage::getLanguage() const
{
    return language_;
}

} // namespace ase
