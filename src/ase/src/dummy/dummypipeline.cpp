#include "dummypipeline.h"

#include "program.h"

namespace ase
{

DummyPipeline::DummyPipeline(Program program, bool blendEnabled) :
    program_(std::move(program)),
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

} // namespace ase
