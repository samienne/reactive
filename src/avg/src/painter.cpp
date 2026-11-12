#include "painter.h"

#include "rendering.h"
#include "brush.h"
#include "pen.h"
#include "glyphatlas.h"

#include <ase/window.h>
#include <ase/matrix.h>
#include <ase/namedvertexspec.h>
#include <ase/shader.h>
#include <ase/shaderinterface.h>
#include <ase/program.h>
#include <ase/buffer.h>
#include <ase/blendmode.h>
#include <ase/pipelinedesc.h>
#include <ase/renderqueue.h>
#include <ase/usage.h>

#include <tracy/Tracy.hpp>

#include <string>

static char const* simpleVsSource =
"#version 330\n"
"\n"
"layout(std140) uniform MatrixBlock\n"
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
"\n"
"layout(std140) uniform MatrixBlock\n"
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
        + "\n"
        + "uniform sampler2D atlas;\n"
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

static char const* presentVsSource =
"#version 330\n"
"\n"
"in vec2 pos;\n"
"in vec2 uv;\n"
"\n"
"out vec2 vUv;\n"
"\n"
"void main()\n"
"{\n"
"   vUv = uv;\n"
"   gl_Position = vec4(pos, 0.0, 1.0);\n"
"}\n";

static char const* presentFsSource =
"#version 330\n"
"\n"
"uniform sampler2D source;\n"
"\n"
"in vec2 vUv;\n"
"out vec4 result;\n"
"\n"
"void main()\n"
"{\n"
"   result = texture(source, vUv);\n"
"}\n";

namespace avg
{

namespace
{
    // Present vertex: xyuv, clip space over texture space
    constexpr float presentQuadVertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };

    constexpr uint32_t presentQuadIndices[] = { 0, 1, 2, 0, 2, 3 };

    ase::Shader makeShader(ase::RenderContext& context, ase::ShaderStage stage,
            std::string const& source)
    {
        return context.makeShader(stage, ase::ShaderLanguage::Glsl330,
                ase::Buffer(source.data(), source.size()));
    }

    ase::ShaderInterface matrixInterface()
    {
        ase::ShaderInterface iface;
        iface.attribute("pos", 0, ase::TypeFloat, 3)
            .attribute("color", 1, ase::TypeFloat, 4)
            .block("MatrixBlock", 0, ase::BlockLayout::Std140, {
                    { "worldViewProj", ase::UniformType::Mat4 },
                });

        return iface;
    }

    ase::Pipeline makePipeline(ase::RenderContext& context, bool solid)
    {
        ase::NamedVertexSpec namedSpec;
        namedSpec.add("color", 4, ase::TypeFloat, false)
            .add("pos", 3, ase::TypeFloat, false);
        auto vs = makeShader(context, ase::ShaderStage::Vertex,
                simpleVsSource);
        auto fs = makeShader(context, ase::ShaderStage::Fragment,
                simpleFsSource);
        auto program = context.makeProgram(std::move(vs), std::move(fs),
                matrixInterface());
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
        auto vs = makeShader(context, ase::ShaderStage::Vertex,
                textVsSource);
        auto fs = makeShader(context, ase::ShaderStage::Fragment,
                textFsSource());
        auto iface = matrixInterface();
        iface.attribute("uv", 2, ase::TypeFloat, 2)
            .sampler("atlas", 0);
        auto program = context.makeProgram(std::move(vs), std::move(fs),
                std::move(iface));
        ase::VertexSpec spec(program, std::move(namedSpec));

        return context.makePipelineWithBlend(program, spec,
                ase::BlendMode::One, ase::BlendMode::OneMinusSrcAlpha);
    }

    ase::ShaderInterface presentInterface()
    {
        ase::ShaderInterface iface;
        iface.attribute("pos", 0, ase::TypeFloat, 2)
            .attribute("uv", 1, ase::TypeFloat, 2)
            .sampler("source", 0);

        return iface;
    }

    ase::Pipeline makePresentPipeline(ase::RenderContext& context)
    {
        ase::NamedVertexSpec namedSpec;
        namedSpec.add("pos", 2, ase::TypeFloat, false)
            .add("uv", 2, ase::TypeFloat, false);
        auto vs = makeShader(context, ase::ShaderStage::Vertex,
                presentVsSource);
        auto fs = makeShader(context, ase::ShaderStage::Fragment,
                presentFsSource);
        auto program = context.makeProgram(std::move(vs), std::move(fs),
                presentInterface());
        ase::VertexSpec spec(program, std::move(namedSpec));

        ase::PipelineDesc desc;
        desc.depthTest = false;
        desc.depthWrite = false;

        return context.makePipeline(program, spec, desc);
    }
} // anonymous namespace

Painter::Painter(pmr::memory_resource* memory, ase::RenderContext& context) :
    memory_(memory),
    renderContext_(context),
    solidPipeline_(makePipeline(context, true)),
    transparentPipeline_(makePipeline(context, false)),
    textPipeline_(makeTextPipeline(context)),
    presentPipeline_(makePresentPipeline(context)),
    presentUniformSet_(context.makeUniformSet(presentInterface())),
    glyphAtlas_(std::make_unique<GlyphAtlas>(context)),
    matrices_(matrixInterface(), "MatrixBlock", {
            ase::blockMember<&MatrixBlock::worldViewProj>("worldViewProj"),
        }),
    uniformRing_(context),
    uniformSet_(context.makeUniformSet(matrixInterface()))
{
    matrices_->worldViewProj = ase::Matrix4f::Identity();
}

Painter::~Painter()
{
}

Painter::Painter(Painter&&) noexcept = default;

pmr::memory_resource* Painter::getResource() const
{
    return memory_;
}

void Painter::setSize(ase::Vector2i pixelSize, float scalingFactor)
{
    ase::Matrix4f m = ase::Matrix4f::Identity();
    m(0,0) = 2.0f * scalingFactor / (float)pixelSize[0];
    m(1,1) = 2.0f * scalingFactor / (float)pixelSize[1];
    m(0,3) = -1.0f;
    m(1,3) = -1.0f;

    matrices_->worldViewProj = m;

    if (frameOpen_)
        writeMatrices();
}

void Painter::beginFrame()
{
    if (frameOpen_)
        return;

    uniformRing_.beginFrame();
    frameOpen_ = true;
    writeMatrices();
}

void Painter::writeMatrices()
{
    uniformSet_.bindUniformBufferRange("MatrixBlock",
            uniformRing_.write(matrices_));
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
    beginFrame();
    commandBuffer_.pushClear(target.getFramebuffer());
}

void Painter::clearWindow(ase::Window& target)
{
    ZoneScoped;
    beginFrame();
    commandBuffer_.pushClear(windowTarget(target).getFramebuffer());
}

void Painter::paintToImage(TargetImage& target, float scalingFactor,
        Drawing const& drawing)
{
    ZoneScoped;
    beginFrame();
    render(memory_, commandBuffer_, renderContext_, target.getFramebuffer(),
            target.getSize(), scalingFactor, *this, drawing);
}

void Painter::paintToWindow(ase::Window& target, Drawing const& drawing)
{
    ZoneScoped;

    beginFrame();
    render(memory_, commandBuffer_, renderContext_,
            windowTarget(target).getFramebuffer(), target.getSize(),
            target.getScalingFactor(), *this, drawing);
}

void Painter::presentWindow(ase::Window& target)
{
    ZoneScoped;

    if (windowTarget_)
        pushPresent(target);

    flush();
    target.present();
}

TargetImage& Painter::windowTarget(ase::Window& window)
{
    auto const pixelSize = window.getScaledSize();
    if (!windowTarget_ || windowTarget_->getSize() != pixelSize)
    {
        windowTarget_.emplace(renderContext_, pixelSize,
                ase::FORMAT_SRGBA);
    }

    return *windowTarget_;
}

void Painter::pushPresent(ase::Window& window)
{
    if (!presentVertices_)
    {
        presentVertices_ = renderContext_.makeVertexBuffer();
        presentIndices_ = renderContext_.makeIndexBuffer();

        commandBuffer_.pushUpload(*presentVertices_,
                ase::Buffer(presentQuadVertices, sizeof(presentQuadVertices)),
                ase::Usage::StaticDraw);
        commandBuffer_.pushUpload(*presentIndices_,
                ase::Buffer(presentQuadIndices, sizeof(presentQuadIndices)),
                ase::Usage::StaticDraw);
    }

    // The quad samples what this frame drew, so the sort must not move it
    // ahead of those draws.
    commandBuffer_.pushBarrier();
    commandBuffer_.push(window.getDefaultFramebuffer(), presentPipeline_,
            presentUniformSet_, *presentVertices_, *presentIndices_,
            { windowTarget_->getTexture() }, 0.0f);
}

void Painter::flush()
{
    ZoneScoped;

    if (frameOpen_)
    {
        ase::CommandBuffer uploads;
        uniformRing_.endFrame(uploads);
        frameOpen_ = false;

        if (uploads.size() > 0)
            renderContext_.getMainRenderQueue().submit(std::move(uploads));
    }

    if (commandBuffer_.size() > 0)
    {
        renderContext_.getMainRenderQueue().submit(std::move(commandBuffer_));
        commandBuffer_ = ase::CommandBuffer();
    }
}

} // namespace
