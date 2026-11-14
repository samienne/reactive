#include "effectshader.h"

#include <stdexcept>

namespace avg
{
namespace
{
    char const* const versionLine = "#version 330\n";

    char const* const passBlockSource =
        "layout(std140) uniform PassBlock\n"
        "{\n"
        "    vec2 targetSize;\n"
        "    float targetDivisor;\n"
        "    vec4 region;\n"
        "    vec4 texel0;\n"
        "    vec4 texel1;\n"
        "    vec4 texel2;\n"
        "    vec4 texel3;\n"
        "} pass;\n";

    char const* const helperSource =
        "out vec4 result;\n"
        "\n"
        "vec2 screenPos()\n"
        "{\n"
        "    return gl_FragCoord.xy * pass.targetDivisor;\n"
        "}\n"
        "\n"
        "vec2 inputUv(vec4 texel)\n"
        "{\n"
        "    return screenPos() * texel.xy / texel.z;\n"
        "}\n";

    void checkInputCount(size_t inputCount)
    {
        if (inputCount > maxPassInputs)
        {
            throw std::invalid_argument("a pass reads "
                    + std::to_string(inputCount) + " inputs; the most is "
                    + std::to_string(maxPassInputs));
        }
    }

    std::string paramBlockSource(std::vector<PassParameter> const& parameters)
    {
        if (parameters.empty())
            return std::string();

        std::string source = "layout(std140) uniform ParamBlock\n{\n";
        for (PassParameter const& parameter : parameters)
        {
            source += "    " + std::string(ase::toString(parameter.type))
                + " " + parameter.name;
            if (parameter.arraySize > 0)
                source += "[" + std::to_string(parameter.arraySize) + "]";
            source += ";\n";
        }

        return source + "} param;\n\n";
    }
} // anonymous namespace

std::string const& passVertexSource()
{
    static std::string const source = std::string(versionLine) + "\n"
        + passBlockSource + "\n"
        "in vec2 pos;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(pos / pass.targetSize * 2.0 - 1.0,"
        " 0.0, 1.0);\n"
        "}\n";

    return source;
}

std::string passInputName(size_t index)
{
    return "input" + std::to_string(index);
}

std::string passTexelName(size_t index)
{
    return "texel" + std::to_string(index);
}

ase::ShaderInterface passInterface(size_t inputCount,
        std::vector<PassParameter> const& parameters)
{
    checkInputCount(inputCount);

    ase::ShaderInterface iface;
    iface.attribute("pos", 0, ase::TypeFloat, 2)
        .block("PassBlock", 0, ase::BlockLayout::Std140, {
                { "targetSize", ase::UniformType::Float2 },
                { "targetDivisor", ase::UniformType::Float },
                { "region", ase::UniformType::Float4 },
                { passTexelName(0), ase::UniformType::Float4 },
                { passTexelName(1), ase::UniformType::Float4 },
                { passTexelName(2), ase::UniformType::Float4 },
                { passTexelName(3), ase::UniformType::Float4 },
                });

    if (!parameters.empty())
        iface.block("ParamBlock", 1, ase::BlockLayout::Std140, parameters);

    for (size_t i = 0; i < inputCount; ++i)
        iface.sampler(passInputName(i), static_cast<int>(i));

    return iface;
}

std::string passFragmentPrelude(size_t inputCount,
        std::vector<PassParameter> const& parameters)
{
    checkInputCount(inputCount);

    std::string source = std::string(versionLine) + "\n"
        + passBlockSource + "\n"
        + paramBlockSource(parameters);

    for (size_t i = 0; i < inputCount; ++i)
        source += "uniform sampler2D " + passInputName(i) + ";\n";

    if (inputCount > 0)
        source += "\n";

    return source + helperSource + "\n";
}

PassShader makePassShader(size_t inputCount,
        std::vector<PassParameter> const& parameters, std::string body)
{
    PassShader shader;
    shader.fragmentSource = passFragmentPrelude(inputCount, parameters)
        + std::move(body);
    shader.interface = passInterface(inputCount, parameters);
    return shader;
}

} // namespace avg
