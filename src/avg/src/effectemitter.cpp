#include "effectemitter.h"

#include "effectshader.h"

#include <ase/buffer.h>
#include <ase/rendercommand.h>
#include <ase/scissorrect.h>
#include <ase/uniformbufferrange.h>
#include <ase/uniformset.h>
#include <ase/usage.h>
#include <ase/vertexbuffer.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace avg
{

namespace
{
    PassShader const& snapshotShader()
    {
        static PassShader const shader = makePassShader(1, {},
                "void main()\n"
                "{\n"
                "    result = texture(input0, inputUv(pass.texel0));\n"
                "}\n");

        return shader;
    }

    std::vector<float> quadVertices(PixelRect const& region)
    {
        float const x0 = static_cast<float>(region.x);
        float const y0 = static_cast<float>(region.y);
        float const x1 = static_cast<float>(region.x + region.width);
        float const y1 = static_cast<float>(region.y + region.height);

        return {
            x0, y0, x1, y0, x1, y1,
            x0, y0, x1, y1, x0, y1,
        };
    }

    std::vector<float> triangleVertices(std::vector<Vector2f> const& triangles)
    {
        std::vector<float> vertices;
        vertices.reserve(triangles.size() * 2);
        for (Vector2f const& v : triangles)
        {
            vertices.push_back(v[0]);
            vertices.push_back(v[1]);
        }

        return vertices;
    }

    float* texelOf(PassBlock& block, size_t index)
    {
        switch (index)
        {
            case 0: return block.texel0;
            case 1: return block.texel1;
            case 2: return block.texel2;
            default: return block.texel3;
        }
    }

    ase::ScissorRect scissorOf(PixelRect const& region)
    {
        return ase::ScissorRect{ region.x, region.y, region.width,
            region.height };
    }
} // anonymous namespace

EffectEmitter::EffectEmitter(ase::RenderContext& context,
        ase::CommandBuffer& commands, ase::UniformRing& uniforms,
        ScratchPool& scratch, PassPipelineCache& pipelines,
        TargetImage& mainTarget) :
    context_(context),
    commands_(commands),
    uniforms_(uniforms),
    scratch_(scratch),
    pipelines_(pipelines),
    mainTarget_(mainTarget),
    passBlock_(passInterface(0, {}), "PassBlock", {
            ase::blockMember<&PassBlock::targetSize>("targetSize"),
            ase::blockMember<&PassBlock::targetDivisor>("targetDivisor"),
            ase::blockMember<&PassBlock::region>("region"),
            ase::blockMember<&PassBlock::texel0>(passTexelName(0)),
            ase::blockMember<&PassBlock::texel1>(passTexelName(1)),
            ase::blockMember<&PassBlock::texel2>(passTexelName(2)),
            ase::blockMember<&PassBlock::texel3>(passTexelName(3)),
            })
{
}

EffectEmitter::~EffectEmitter()
{
}

void EffectEmitter::emitSnapshot(ScratchSlot const& slot,
        PixelRect const& region)
{
    if (region.isEmpty())
        return;

    ase::Format const format = mainTarget_.getTexture().getFormat();
    PassPipeline& pipeline = pipelines_.getPipeline(snapshotShader(),
            Geometry::Region, format);
    ScratchTarget target = scratch_.acquire(slot);

    std::vector<Input> inputs;
    inputs.push_back(Input{ mainTarget_.getTexture(), mainTarget_.getSize(),
            Divisor::One });

    pushBarrier();
    draw(pipeline, target.framebuffer, target.size, slot.key.divisor, region,
            inputs, std::nullopt, quadVertices(region));
    ++snapshotCount_;
    pushBarrier();
}

void EffectEmitter::emitEffect(Effect const& effect,
        EffectRegions const& regions, EffectAllocation const& allocation,
        std::vector<Vector2f> const& shapeTriangles)
{
    std::vector<Pass> const& passes = effect.getPasses();
    if (regions.passRegions.size() != passes.size()
            || allocation.passes.size() != passes.size())
    {
        throw std::invalid_argument("regions or allocation do not match the"
                " effect's " + std::to_string(passes.size()) + " passes");
    }

    if (regions.passRegions.back().isEmpty())
        return;

    ase::Format const mainFormat = mainTarget_.getTexture().getFormat();

    pushBarrier();

    for (size_t i = 0; i < passes.size(); ++i)
    {
        Pass const& pass = passes[i];
        PixelRect const& region = regions.passRegions[i];
        PassAllocation const& passAllocation = allocation.passes[i];

        if (region.isEmpty())
            continue;

        if (passAllocation.inputs.size() != pass.inputs.size())
        {
            throw std::invalid_argument("pass " + std::to_string(i)
                    + " is allocated "
                    + std::to_string(passAllocation.inputs.size())
                    + " inputs for its " + std::to_string(pass.inputs.size()));
        }

        std::vector<Input> inputs;
        inputs.reserve(pass.inputs.size());
        for (ScratchSlot const& slot : passAllocation.inputs)
        {
            ScratchTarget source = scratch_.acquire(slot);
            inputs.push_back(Input{ std::move(source.texture), source.size,
                    slot.key.divisor });
        }

        if (pass.geometry == Geometry::Shape)
        {
            PassPipeline& pipeline = pipelines_.getPipeline(pass.shader,
                    Geometry::Shape, mainFormat);
            draw(pipeline, mainTarget_.getFramebuffer(), mainTarget_.getSize(),
                    Divisor::One, region, inputs, pass.parameters,
                    triangleVertices(shapeTriangles));
        }
        else
        {
            if (!pass.output || !passAllocation.output)
            {
                throw std::invalid_argument("Region pass "
                        + std::to_string(i) + " has no output");
            }

            PassPipeline& pipeline = pipelines_.getPipeline(pass.shader,
                    Geometry::Region, pass.output->format);
            ScratchTarget target = scratch_.acquire(
                    passAllocation.output->slot);
            draw(pipeline, target.framebuffer, target.size,
                    pass.output->divisor, region, inputs, pass.parameters,
                    quadVertices(region));
        }

        ++passCount_;
        pushBarrier();
    }
}

size_t EffectEmitter::getPassCount() const
{
    return passCount_;
}

size_t EffectEmitter::getSnapshotCount() const
{
    return snapshotCount_;
}

void EffectEmitter::draw(PassPipeline& pipeline, ase::Framebuffer const& target,
        Vector2i targetSize, Divisor divisor, PixelRect const& region,
        std::vector<Input> const& inputs,
        std::optional<ase::UniformValues> const& parameters,
        std::vector<float> const& vertices)
{
    if (inputs.size() != pipeline.getInputCount())
    {
        throw std::invalid_argument("a pass binds "
                + std::to_string(inputs.size()) + " textures for a shader"
                " with " + std::to_string(pipeline.getInputCount())
                + " samplers");
    }

    PassBlock& block = *passBlock_;
    block.targetSize[0] = static_cast<float>(targetSize[0]);
    block.targetSize[1] = static_cast<float>(targetSize[1]);
    block.targetDivisor = static_cast<float>(toInt(divisor));
    block.region[0] = static_cast<float>(region.x);
    block.region[1] = static_cast<float>(region.y);
    block.region[2] = static_cast<float>(region.width);
    block.region[3] = static_cast<float>(region.height);

    std::vector<ase::Texture> textures;
    textures.reserve(inputs.size());
    for (size_t i = 0; i < maxPassInputs; ++i)
    {
        float* texel = texelOf(block, i);
        if (i < inputs.size())
        {
            Input const& input = inputs[i];
            texel[0] = 1.0f / static_cast<float>(input.size[0]);
            texel[1] = 1.0f / static_cast<float>(input.size[1]);
            texel[2] = static_cast<float>(toInt(input.divisor));
            texel[3] = 0.0f;
            textures.push_back(input.texture);
        }
        else
        {
            texel[0] = texel[1] = texel[2] = texel[3] = 0.0f;
        }
    }

    ase::UniformSet uniformSet = pipeline.acquireUniformSet();
    uniformSet.bindUniformBufferRange(pipeline.getPassBlockBinding(),
            uniforms_.write(passBlock_));

    if (parameters && pipeline.getParamBlockBinding())
    {
        uniformSet.bindUniformBufferRange(*pipeline.getParamBlockBinding(),
                uniforms_.write(*parameters));
    }

    ase::VertexBuffer vertexBuffer = context_.makeVertexBuffer();
    commands_.pushUpload(vertexBuffer, ase::Buffer(vertices),
            ase::Usage::StreamDraw);

    commands_.push(target, pipeline.getPipeline(), std::move(uniformSet),
            std::move(vertexBuffer), std::nullopt, std::move(textures), 0.0f,
            scissorOf(region));
}

void EffectEmitter::pushBarrier()
{
    if (commands_.size() > 0
            && std::holds_alternative<ase::BarrierCommand>(
                *(commands_.end() - 1)))
    {
        return;
    }

    commands_.pushBarrier();
}

} // namespace avg
