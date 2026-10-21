#include "painter.h"

#include "rendering.h"
#include "brush.h"
#include "pen.h"
#include "glyphatlas.h"

#include <ase/window.h>
#include <ase/uniformbufferrange.h>
#include <ase/matrix.h>
#include <ase/namedvertexspec.h>
#include <ase/vertexshader.h>
#include <ase/fragmentshader.h>
#include <ase/program.h>
#include <ase/blendmode.h>
#include <ase/renderqueue.h>

#include <tracy/Tracy.hpp>

#include <string>

static char const* simpleVsSource =
"#version 330\n"
"#extension GL_ARB_shading_language_420pack : require\n"
"\n"
"layout(std140, binding = 0) uniform MatrixBlock\n"
"{\n"
"   mat4 worldViewProj;\n"
"} matrices;\n"
"\n"
"in vec4 color;\n"
"in vec3 pos;\n"
"\n"
"out vec4 vColor;\n"
"\n"
"void main()\n"
"{\n"
"   vColor = color;\n"
"   gl_Position = matrices.worldViewProj * vec4(pos, 1.0);\n"
"}\n";

static char const* simpleFsSource =
"#version 330\n"
"in vec4 vColor;\n"
"out vec4 result;\n"
"\n"
"void main()\n"
"{\n"
"   result = vColor;\n"
"}\n";

static char const* textVsSource =
"#version 330\n"
"#extension GL_ARB_shading_language_420pack : require\n"
"\n"
"layout(std140, binding = 0) uniform MatrixBlock\n"
"{\n"
"   mat4 worldViewProj;\n"
"} matrices;\n"
"\n"
"in vec4 color;\n"
"in vec3 pos;\n"
"in vec2 uv;\n"
"\n"
"out vec4 vColor;\n"
"out vec2 vUv;\n"
"\n"
"void main()\n"
"{\n"
"   vColor = color;\n"
"   vUv = uv;\n"
"   gl_Position = matrices.worldViewProj * vec4(pos, 1.0);\n"
"}\n";

// Coverage is raised to 1/textGamma before it scales the premultiplied
// colour, which thickens light-on-dark text that linear coverage leaves thin.
static char const* textGamma = "1.45";

static std::string textFsSource()
{
    return std::string()
        + "#version 330\n"
        + "#extension GL_ARB_shading_language_420pack : require\n"
        + "\n"
        + "layout(binding = 0) uniform sampler2D atlas;\n"
        + "\n"
        + "in vec4 vColor;\n"
        + "in vec2 vUv;\n"
        + "out vec4 result;\n"
        + "\n"
        + "const float gamma = " + textGamma + ";\n"
        + "\n"
        + "void main()\n"
        + "{\n"
        + "   result = vColor * pow(texture(atlas, vUv).r, 1.0 / gamma);\n"
        + "}\n";
}

namespace avg
{

namespace
{
    ase::Pipeline makePipeline(ase::RenderContext& context, bool solid)
    {
        ase::NamedVertexSpec namedSpec;
        namedSpec.add("color", 4, ase::TypeFloat, false)
            .add("pos", 3, ase::TypeFloat, false);
        auto vs = context.makeVertexShader(simpleVsSource);
        auto fs = context.makeFragmentShader(simpleFsSource);
        auto program = context.makeProgram(std::move(vs), std::move(fs));
        ase::VertexSpec spec(program, std::move(namedSpec));

        if (solid)
            return context.makePipeline(program, spec);
        else
        {
            return context.makePipelineWithBlend(program, spec,
                    ase::BlendMode::One, ase::BlendMode::OneMinusSrcAlpha);
        }
    }

    ase::Pipeline makeTextPipeline(ase::RenderContext& context)
    {
        ase::NamedVertexSpec namedSpec;
        namedSpec.add("color", 4, ase::TypeFloat, false)
            .add("pos", 3, ase::TypeFloat, false)
            .add("uv", 2, ase::TypeFloat, false);
        auto vs = context.makeVertexShader(textVsSource);
        auto fs = context.makeFragmentShader(textFsSource());
        auto program = context.makeProgram(std::move(vs), std::move(fs));
        ase::VertexSpec spec(program, std::move(namedSpec));

        return context.makePipelineWithBlend(program, spec,
                ase::BlendMode::One, ase::BlendMode::OneMinusSrcAlpha);
    }
} // anonymous namespace

Painter::Painter(pmr::memory_resource* memory, ase::RenderContext& context) :
    memory_(memory),
    renderContext_(context),
    solidPipeline_(makePipeline(context, true)),
    transparentPipeline_(makePipeline(context, false)),
    textPipeline_(makeTextPipeline(context)),
    glyphAtlas_(std::make_unique<GlyphAtlas>(context)),
    buffer_(),
    uniformSet_(context.makeUniformSet()),
    uniformBuffer_(context.makeUniformBuffer())
{
    uniformSet_.bindUniformBufferRange(0,
            ase::UniformBufferRange{ 0, 16*sizeof(float), uniformBuffer_ });
}

Painter::~Painter()
{
}

Painter::Painter(Painter&&) noexcept = default;

pmr::memory_resource* Painter::getResource() const
{
    return memory_;
}

void Painter::setSize(ase::Vector2i size)
{
    buffer_.resize(16*sizeof(float));
    char* data = buffer_.mapWrite<char>();

    ase::Matrix4f m = ase::Matrix4f::Identity();
    m(0,0) = 2.0f/(float)size[0];
    m(1,1) = 2.0f/(float)size[1];
    m(0,3) = -1.0f;
    m(1,3) = -1.0f;

    std::memcpy(data, m.data(), 16 * sizeof(float));

    commandBuffer_.pushUpload(uniformBuffer_, buffer_, ase::Usage::StreamRead);

    uniformSet_.bindUniformBufferRange(0,
            ase::UniformBufferRange{ 0, 16*sizeof(float), uniformBuffer_ });
}

ase::UniformSet const& Painter::getUniformSet() const
{
    return uniformSet_;
}

ase::Pipeline const& Painter::getPipeline(Brush const& brush) const
{
    if (brush.getColor().getAlpha() > 254.0f/255.0f)
        return solidPipeline_;
    else
        return transparentPipeline_;
}

ase::Pipeline const& Painter::getPipeline(Pen const& pen) const
{
    if (std::abs(pen.getBrush().getColor().getAlpha()) - 1.0f > 0.0001f)
        return solidPipeline_;
    else
        return transparentPipeline_;
}

ase::Pipeline const& Painter::getTextPipeline() const
{
    return textPipeline_;
}

GlyphAtlas& Painter::getGlyphAtlas()
{
    return *glyphAtlas_;
}

void Painter::clearImage(TargetImage& target)
{
    ZoneScoped;
    commandBuffer_.pushClear(target.getFramebuffer());
}

void Painter::clearWindow(ase::Window& target)
{
    ZoneScoped;
    commandBuffer_.pushClear(target.getDefaultFramebuffer());
}

void Painter::paintToImage(TargetImage& target, float scalingFactor,
        Drawing const& drawing)
{
    ZoneScoped;
    render(memory_, commandBuffer_, renderContext_, target.getFramebuffer(),
            target.getSize(), scalingFactor, *this, drawing);
}

void Painter::paintToWindow(ase::Window& target, Drawing const& drawing)
{
    ZoneScoped;

    render(memory_, commandBuffer_, renderContext_,
            target.getDefaultFramebuffer(), target.getSize(),
            target.getScalingFactor(), *this, drawing);
}

void Painter::presentWindow(ase::Window& target)
{
    ZoneScoped;

    flush();
    target.present();
}

void Painter::flush()
{
    ZoneScoped;

    if (commandBuffer_.size() > 0)
    {
        renderContext_.getMainRenderQueue().submit(std::move(commandBuffer_));
        commandBuffer_ = ase::CommandBuffer();
    }
}

} // namespace
