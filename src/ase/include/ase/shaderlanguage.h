#pragma once

#include "asevisibility.h"

#include <stdexcept>
#include <string_view>

namespace ase
{
    /**
     * @brief A shading language dialect a shader's bytes are written in.
     *
     * One value per dialect: GLSL 330 and GLSL 450 are different languages
     * here, so a backend's accepted list compares with ==.
     */
    enum class ShaderLanguage
    {
        Glsl330,
        GlslEs300,
        Glsl450,
        SpirV,
        Wgsl,
        Msl
    };

    ASE_EXPORT std::string_view toString(ShaderLanguage language);

    /**
     * @brief Thrown when a shader is created in a language the context does
     * not list in getShaderLanguages().
     */
    class ASE_EXPORT UnsupportedShaderLanguage : public std::runtime_error
    {
    public:
        explicit UnsupportedShaderLanguage(ShaderLanguage language);

        ShaderLanguage getLanguage() const;

    private:
        ShaderLanguage language_;
    };
}
