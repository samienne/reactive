#include "passpipelinecache.h"

#include "effectshader.h"

#include <ase/blendmode.h>
#include <ase/buffer.h>
#include <ase/namedvertexspec.h>
#include <ase/pipelinedesc.h>
#include <ase/program.h>
#include <ase/shader.h>
#include <ase/shaderlanguage.h>
#include <ase/shaderstage.h>
#include <ase/vertexspec.h>

#include <stdexcept>
#include <tuple>
#include <utility>

namespace avg
{

namespace
{
    ase::Shader makeShader(ase::RenderContext& context, ase::ShaderStage stage,
            std::string const& source)
    {
        return context.makeShader(stage, ase::ShaderLanguage::Glsl330,
                ase::Buffer(source.data(), source.size()));
    }

    ase::PipelineDesc passPipelineDesc(Geometry geometry)
    {
        ase::PipelineDesc desc;
        desc.depthTest = false;
        desc.depthWrite = false;

        if (geometry == Geometry::Shape)
        {
            desc.blend = ase::BlendFunc{ ase::BlendMode::SrcAlpha,
                ase::BlendMode::OneMinusSrcAlpha };
        }

        return desc;
    }

    ase::Pipeline makePassPipeline(ase::RenderContext& context,
            PassShader const& shader, Geometry geometry)
    {
        ase::NamedVertexSpec namedSpec;
        namedSpec.add("pos", 2, ase::TypeFloat, false);

        auto vs = makeShader(context, ase::ShaderStage::Vertex,
                passVertexSource());
        auto fs = makeShader(context, ase::ShaderStage::Fragment,
                shader.fragmentSource);
        auto program = context.makeProgram(std::move(vs), std::move(fs),
                shader.interface);
        ase::VertexSpec spec(program, namedSpec);

        return context.makePipeline(std::move(program), std::move(spec),
                passPipelineDesc(geometry));
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

bool PassPipelineCache::Key::operator<(Key const& rhs) const
{
    return std::tie(format, geometry, fragmentSource)
        < std::tie(rhs.format, rhs.geometry, rhs.fragmentSource);
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
        Geometry geometry, ase::Format targetFormat)
{
    Key key{ shader.fragmentSource, targetFormat, geometry };

    auto i = pipelines_.find(key);
    if (i == pipelines_.end())
    {
        i = pipelines_.emplace(std::move(key),
                PassPipeline(context_,
                    makePassPipeline(context_, shader, geometry),
                    shader.interface)).first;
    }

    return i->second;
}

size_t PassPipelineCache::getPipelineCount() const
{
    return pipelines_.size();
}

} // namespace avg
