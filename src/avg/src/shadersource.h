#pragma once

#include <avg/avgvisibility.h>

#include <ase/buffer.h>
#include <ase/program.h>
#include <ase/rendercontext.h>
#include <ase/shaderinterface.h>
#include <ase/shaderlanguage.h>

#include <string>
#include <vector>

namespace avg
{
    /**
     * @brief A program as avg writes it: GLSL 330 for each stage and the
     * interface the stages are declared against.
     */
    struct AVG_EXPORT ShaderSource
    {
        std::string vertex;
        std::string fragment;
        ase::ShaderInterface iface;
    };

    /**
     * @brief A program's stages as bytes in one language, with the
     * interface they are declared against.
     */
    struct AVG_EXPORT ShaderBytes
    {
        ase::ShaderLanguage language;
        ase::Buffer vertex;
        ase::Buffer fragment;
        ase::ShaderInterface iface;
    };

    /**
     * @brief The language avg hands its shaders to a context in.
     *
     * Throws ase::UnsupportedShaderLanguage, naming GLSL 330, when
     * `supported` lacks GLSL 330.
     */
    AVG_EXPORT ase::ShaderLanguage selectShaderLanguage(
            std::vector<ase::ShaderLanguage> const& supported);

    /**
     * @brief `source` as the bytes `context` compiles, in the language
     * selectShaderLanguage picks from the context's list.
     *
     *     ShaderBytes bytes = shaderBytes(context,
     *             { vertexGlsl, fragmentGlsl, iface });
     *     context.makeShader(ase::ShaderStage::Vertex, bytes.language,
     *             bytes.vertex);
     */
    AVG_EXPORT ShaderBytes shaderBytes(ase::RenderContext const& context,
            ShaderSource const& source);

    /**
     * @brief A program linked from `source` on `context`, through
     * shaderBytes.
     */
    AVG_EXPORT ase::Program makeProgram(ase::RenderContext& context,
            ShaderSource const& source);
}
