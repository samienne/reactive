#pragma once

#include "effect.h"
#include "avgvisibility.h"

#include <ase/shaderinterface.h>

#include <cstddef>
#include <string>
#include <vector>

namespace avg
{
    /**
     * @brief A member of a pass's ParamBlock: a value the effect sets and
     * the fragment shader reads as param.<name>.
     */
    using PassParameter = ase::ShaderInterface::MemberDecl;

    /**
     * @brief The most inputs one pass can read.
     */
    constexpr size_t maxPassInputs = 4;

    /**
     * @brief The vertex shader every effect pass is linked with.
     *
     * It takes one attribute, pos, a vec2 in pixels of the pass's target
     * with the origin at the target's first texel, and maps it to clip
     * space with PassBlock.targetSize. A Region pass draws a quad over its
     * region with it, the Shape pass the shape's triangles.
     */
    AVG_EXPORT std::string const& passVertexSource();

    /**
     * @brief The sampler pass input index is read through, "input0" for
     * the first; it is bound to texture unit index.
     */
    AVG_EXPORT std::string passInputName(size_t index);

    /**
     * @brief The PassBlock member describing pass input index, "texel0"
     * for the first.
     */
    AVG_EXPORT std::string passTexelName(size_t index);

    /**
     * @brief Declares the interface of a pass shader with inputCount
     * inputs and the given parameters.
     *
     *     ase::ShaderInterface iface = passInterface(1, {
     *             { "tint", ase::UniformType::Float4 },
     *             });
     *
     * Every pass gets the attribute pos at location 0, the std140 block
     * PassBlock at binding 0 and one sampler2D per input, named by
     * passInputName and bound to the unit of the input's index. ParamBlock,
     * at binding 1, holds the parameters in the given order and is left out
     * when there are none. Throws std::invalid_argument when inputCount
     * exceeds maxPassInputs.
     *
     * PassBlock is filled by the renderer before every draw of a pass:
     *
     * - vec2 targetSize: the pass's target in its own pixels.
     * - float targetDivisor: screen pixels per target pixel; 1 for the
     *   Shape pass.
     * - vec4 region: the origin (xy) and size (zw) of the pass's region in
     *   target pixels.
     * - vec4 texel0 to texel3: for input i, xy is one texel of the input's
     *   texture in its own uv (1 / texture size) and z the input's divisor,
     *   screen pixels per texel; w is unused and the members of inputs the
     *   pass lacks are zero.
     *
     * The same declaration is in the vertex shader, so every program
     * links the block identically.
     */
    AVG_EXPORT ase::ShaderInterface passInterface(size_t inputCount,
            std::vector<PassParameter> const& parameters);

    /**
     * @brief The GLSL a pass fragment shader opens with: the version line,
     * the blocks and samplers passInterface declares, the output
     * `out vec4 result`, and two helpers.
     *
     * `vec2 screenPos()` is the fragment's centre in screen pixels,
     * `gl_FragCoord.xy * pass.targetDivisor`. `vec2 inputUv(vec4 texel)`
     * maps it to the uv of the input the texel member describes,
     * `screenPos() * texel.xy / texel.z`. A fragment lands on the centre
     * of the matching texel when the divisors agree and on the corner
     * shared by the middle 2x2 input texels of its block when the input is
     * finer, so one bilinear fetch averages those four texels: the whole
     * block when the input is twice as fine, a part of it beyond that.
     * Nothing else about the fragment's position is assumed, so a fragment
     * shader works with any vertex stage that lands fragments on the
     * target's pixels.
     */
    AVG_EXPORT std::string passFragmentPrelude(size_t inputCount,
            std::vector<PassParameter> const& parameters);

    /**
     * @brief A PassShader whose source is the prelude followed by body,
     * with the interface the prelude declares.
     *
     *     PassShader copy = makePassShader(1, {},
     *             "void main()\n"
     *             "{\n"
     *             "    result = texture(input0, inputUv(pass.texel0));\n"
     *             "}\n");
     */
    AVG_EXPORT PassShader makePassShader(size_t inputCount,
            std::vector<PassParameter> const& parameters, std::string body);
}
