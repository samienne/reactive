#include "builtineffects.h"

#include "effectshader.h"

#include <ase/uniformvalues.h>
#include <ase/vector.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace avg
{
namespace
{
    constexpr float maxBlurPassRadius = 16.0f;

    char const* const copyBody =
        "void main()\n"
        "{\n"
        "    result = texture(input0, inputUv(pass.texel0));\n"
        "}\n";

    // blur.xy steps one tap along the blur's axis in texels of the
    // input, blur.z is the tap radius and blur.w the Gaussian's sigma,
    // both in texels.
    char const* const blurBody =
        "void main()\n"
        "{\n"
        "    vec2 uv = inputUv(pass.texel0);\n"
        "    vec2 tapStep = param.blur.xy * pass.texel0.xy;\n"
        "    int taps = int(param.blur.z);\n"
        "    float sigma = param.blur.w;\n"
        "    vec4 sum = vec4(0.0);\n"
        "    float weightSum = 0.0;\n"
        "    for (int i = -taps; i <= taps; ++i)\n"
        "    {\n"
        "        float x = float(i);\n"
        "        float weight = exp(-x * x / (2.0 * sigma * sigma));\n"
        "        sum += weight * texture(input0, uv + x * tapStep);\n"
        "        weightSum += weight;\n"
        "    }\n"
        "    result = sum / weightSum;\n"
        "}\n";

    // tint.rgb and highlight.rgb are colours with their strengths in a;
    // adjust.x is the saturation scale and adjust.y the highlight's width
    // in target pixels, measured inwards from the pass's region.
    char const* const glassBody =
        "void main()\n"
        "{\n"
        "    vec3 colour = texture(input0, inputUv(pass.texel0)).rgb;\n"
        "    float grey = dot(colour, vec3(0.2126, 0.7152, 0.0722));\n"
        "    colour = mix(vec3(grey), colour, param.adjust.x);\n"
        "    colour = mix(colour, param.tint.rgb, param.tint.a);\n"
        "    vec2 p = gl_FragCoord.xy - pass.region.xy;\n"
        "    float edge = min(min(p.x, p.y),\n"
        "            min(pass.region.z - p.x, pass.region.w - p.y));\n"
        "    float rim = param.highlight.a\n"
        "            * (1.0 - smoothstep(0.0, param.adjust.y, edge));\n"
        "    colour = mix(colour, param.highlight.rgb, rim);\n"
        "    result = vec4(colour, 1.0);\n"
        "}\n";

    char const* const tintBody =
        "void main()\n"
        "{\n"
        "    vec3 colour = texture(input0, inputUv(pass.texel0)).rgb;\n"
        "    result = vec4(mix(colour, param.tint.rgb, param.tint.a), 1.0);\n"
        "}\n";

    ase::Vector4f toVector(Color const& colour, float w)
    {
        return ase::Vector4f(colour.getRed(), colour.getGreen(),
                colour.getBlue(), w);
    }

    void requireNonNegative(float value, char const* name)
    {
        if (value < 0.0f)
        {
            throw std::invalid_argument(std::string(name)
                    + " must not be negative");
        }
    }

    void requireStrength(float value, char const* name)
    {
        if (value < 0.0f || value > 1.0f)
        {
            throw std::invalid_argument(std::string(name)
                    + " must be within [0, 1]");
        }
    }

    Pass regionPass(PassShader shader, std::vector<PassInput> inputs,
            Divisor divisor, int padding)
    {
        Pass pass;
        pass.shader = std::move(shader);
        pass.inputs = std::move(inputs);
        pass.output = PassOutput{ divisor, ase::FORMAT_RGBA16F };
        pass.geometry = Geometry::Region;
        pass.padding = padding;
        return pass;
    }

    Pass shapePass(PassShader shader, std::vector<PassInput> inputs,
            int padding)
    {
        Pass pass;
        pass.shader = std::move(shader);
        pass.inputs = std::move(inputs);
        pass.geometry = Geometry::Shape;
        pass.padding = padding;
        return pass;
    }

    Pass blurPass(PassInput input, Divisor divisor, int tapRadius,
            float sigma, ase::Vector2f direction)
    {
        PassShader shader = makePassShader(1, {
                { "blur", ase::UniformType::Float4 },
                }, blurBody);

        ase::UniformValues values(shader.interface, "ParamBlock");
        values.set("blur", ase::Vector4f(direction.x(), direction.y(),
                    static_cast<float>(tapRadius), sigma));

        Pass pass = regionPass(std::move(shader), { input }, divisor,
                tapRadius);
        pass.parameters = std::move(values);
        return pass;
    }
} // anonymous namespace

Divisor frostedGlassDivisor(float radius)
{
    if (radius <= 2.0f * maxBlurPassRadius)
        return Divisor::Two;
    if (radius <= 4.0f * maxBlurPassRadius)
        return Divisor::Four;

    return Divisor::Eight;
}

Effect frostedGlass(FrostedGlassOptions const& options)
{
    requireNonNegative(options.radius, "radius");
    requireNonNegative(options.saturation, "saturation");
    requireNonNegative(options.highlightWidth, "highlightWidth");
    requireStrength(options.tintStrength, "tintStrength");
    requireStrength(options.highlightStrength, "highlightStrength");

    Divisor const divisor = frostedGlassDivisor(options.radius);
    float const passRadius = options.radius / static_cast<float>(
            toInt(divisor));
    int const tapRadius = static_cast<int>(std::ceil(passRadius));
    float const sigma = std::max(passRadius / 2.0f, 0.5f);

    PassShader glass = makePassShader(1, {
            { "tint", ase::UniformType::Float4 },
            { "highlight", ase::UniformType::Float4 },
            { "adjust", ase::UniformType::Float4 },
            }, glassBody);

    ase::UniformValues values(glass.interface, "ParamBlock");
    values.set("tint", toVector(options.tint, options.tintStrength));
    values.set("highlight", toVector(options.highlight,
                options.highlightStrength));
    values.set("adjust", ase::Vector4f(options.saturation,
                options.highlightWidth, 0.0f, 0.0f));

    // Bilinear upsampling reaches half a blurred texel past the shape.
    Pass final = shapePass(std::move(glass), { PassInput::pass(2) },
            toInt(divisor) / 2);
    final.parameters = std::move(values);

    std::vector<Pass> passes;
    passes.push_back(regionPass(makePassShader(1, {}, copyBody),
            { PassInput::backdrop() }, divisor, 0));
    passes.push_back(blurPass(PassInput::pass(0), divisor, tapRadius, sigma,
            ase::Vector2f(1.0f, 0.0f)));
    passes.push_back(blurPass(PassInput::pass(1), divisor, tapRadius, sigma,
            ase::Vector2f(0.0f, 1.0f)));
    passes.push_back(std::move(final));

    return Effect(std::move(passes));
}

Effect tint(Color const& colour, float strength)
{
    requireStrength(strength, "strength");

    PassShader shader = makePassShader(1, {
            { "tint", ase::UniformType::Float4 },
            }, tintBody);

    ase::UniformValues values(shader.interface, "ParamBlock");
    values.set("tint", toVector(colour, strength * colour.getAlpha()));

    Pass pass = shapePass(std::move(shader), { PassInput::backdrop() }, 0);
    pass.parameters = std::move(values);

    std::vector<Pass> passes;
    passes.push_back(std::move(pass));

    return Effect(std::move(passes));
}

} // namespace avg
