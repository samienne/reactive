#include "dummypipeline.h"

#include "program.h"

namespace ase
{

DummyPipeline::DummyPipeline(Program program) :
    program_(std::move(program))
{
}

Program const& DummyPipeline::getProgram() const
{
    return program_;
}

} // namespace ase
