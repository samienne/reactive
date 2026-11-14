#include "dummypipeline.h"

#include "program.h"

namespace ase
{

DummyPipeline::DummyPipeline(Program program, VertexSpec vertexSpec,
        PipelineDesc desc) :
    program_(std::move(program)),
    vertexSpec_(std::move(vertexSpec)),
    desc_(desc)
{
}

Format DummyPipeline::getColorFormat() const
{
    return desc_.colorFormat;
}

std::optional<Format> DummyPipeline::getDepthFormat() const
{
    return desc_.depthFormat;
}

bool DummyPipeline::isBlendEnabled() const
{
    return desc_.blend.has_value();
}

bool DummyPipeline::isDepthTestEnabled() const
{
    return desc_.depthTest;
}

bool DummyPipeline::isDepthWriteEnabled() const
{
    return desc_.writesDepth();
}

Program const& DummyPipeline::getProgram() const
{
    return program_;
}

VertexSpec const& DummyPipeline::getVertexSpec() const
{
    return vertexSpec_;
}

} // namespace ase
