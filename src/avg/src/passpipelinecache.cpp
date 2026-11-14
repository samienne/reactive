#include "passpipelinecache.h"

#include "effectshader.h"
#include "shadersource.h"

#include <ase/blendmode.h>
#include <ase/namedvertexspec.h>
#include <ase/pipelinedesc.h>
#include <ase/program.h>
#include <ase/vertexspec.h>

#include <stdexcept>
#include <tuple>
#include <utility>

namespace avg
{

namespace
{
    ase::PipelineDesc passPipelineDesc(Geometry geometry,
            ase::Format colorFormat, std::optional<ase::Format> depthFormat)
    {
        ase::PipelineDesc desc{ colorFormat, depthFormat };
        desc.depthTest = false;
        desc.depthWrite = false;

        if (geometry == Geometry::Shape)
        {
            desc.blend = ase::BlendFunc{ ase::BlendMode::SrcAlpha,
                ase::BlendMode::OneMinusSrcAlpha };
        }

        return desc;
    }

    ase::Program makePassProgram(ase::RenderContext& context,
            PassShader const& shader)
    {
        return makeProgram(context,
                { passVertexSource(), shader.fragmentSource,
                    shader.interface });
    }

    ase::Pipeline makePassPipeline(ase::RenderContext& context,
            ase::Program program, Geometry geometry, ase::Format colorFormat,
            std::optional<ase::Format> depthFormat)
    {
        ase::NamedVertexSpec namedSpec;
        namedSpec.add("pos", 2, ase::TypeFloat, false);
        ase::VertexSpec spec(program, namedSpec);

        return context.makePipeline(std::move(program), std::move(spec),
                passPipelineDesc(geometry, colorFormat, depthFormat));
    }
} // anonymous namespace

PassPipeline::PassPipeline(ase::RenderContext& context, ase::Pipeline pipeline,
        ase::ShaderInterface iface) :
    context_(context),
    pipeline_(std::move(pipeline)),
    interface_(std::move(iface)),
    inputCount_(interface_.getSamplers().size())
{
    ase::ShaderInterface::Block const* passBlock = interface_.findBlock(
            "PassBlock");
    if (!passBlock)
        throw std::invalid_argument("a pass shader declares no PassBlock");
    passBlockBinding_ = passBlock->binding;

    if (ase::ShaderInterface::Block const* paramBlock = interface_.findBlock(
                "ParamBlock"))
    {
        paramBlockBinding_ = paramBlock->binding;
    }
}

PassPipeline::PassPipeline(PassPipeline&&) noexcept = default;

PassPipeline::~PassPipeline()
{
}

ase::Pipeline const& PassPipeline::getPipeline() const
{
    return pipeline_;
}

int PassPipeline::getPassBlockBinding() const
{
    return passBlockBinding_;
}

std::optional<int> PassPipeline::getParamBlockBinding() const
{
    return paramBlockBinding_;
}

size_t PassPipeline::getInputCount() const
{
    return inputCount_;
}

ase::UniformSet PassPipeline::acquireUniformSet()
{
    if (usedSets_ == uniformSets_.size())
        uniformSets_.push_back(context_.makeUniformSet(interface_));

    return uniformSets_[usedSets_++];
}

void PassPipeline::beginFrame()
{
    usedSets_ = 0;
}

bool PassPipelineCache::PipelineKey::operator<(PipelineKey const& rhs) const
{
    return std::tie(program, geometry, colorFormat, depthFormat)
        < std::tie(rhs.program, rhs.geometry, rhs.colorFormat,
                rhs.depthFormat);
}

PassPipelineCache::PassPipelineCache(ase::RenderContext& context) :
    context_(context)
{
}

PassPipelineCache::PassPipelineCache(PassPipelineCache&&) noexcept = default;

PassPipelineCache::~PassPipelineCache()
{
}

void PassPipelineCache::beginFrame()
{
    for (auto& entry : pipelines_)
        entry.second.beginFrame();
}

PassPipeline& PassPipelineCache::getPipeline(PassShader const& shader,
        Geometry geometry, ase::Format colorFormat,
        std::optional<ase::Format> depthFormat)
{
    PipelineKey key{ getProgram(shader), geometry, colorFormat, depthFormat };

    auto i = pipelines_.find(key);
    if (i == pipelines_.end())
    {
        ase::Pipeline pipeline = makePassPipeline(context_, key.program,
                geometry, colorFormat, depthFormat);
        i = pipelines_.emplace(std::move(key),
                PassPipeline(context_, std::move(pipeline),
                    shader.interface)).first;
    }

    return i->second;
}

ase::Program const& PassPipelineCache::getProgram(PassShader const& shader)
{
    auto i = programs_.find(shader.fragmentSource);
    if (i == programs_.end())
    {
        i = programs_.emplace(shader.fragmentSource,
                makePassProgram(context_, shader)).first;
    }

    return i->second;
}

size_t PassPipelineCache::getPipelineCount() const
{
    return pipelines_.size();
}

} // namespace avg
