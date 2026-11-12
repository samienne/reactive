#include "dummypipeline.h"

#include "program.h"

namespace ase
{

DummyPipeline::DummyPipeline(Program program, VertexSpec vertexSpec,
        bool blendEnabled) :
    program_(std::move(program)),
    vertexSpec_(std::move(vertexSpec)),
    blendEnabled_(blendEnabled)
{
}

bool DummyPipeline::isBlendEnabled() const
{
    return blendEnabled_;
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
