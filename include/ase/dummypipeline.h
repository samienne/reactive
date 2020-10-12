#pragma once

#include "pipelineimpl.h"

namespace ase
{
    class DummyPipeline : public PipelineImpl
    {
    public:
        explicit DummyPipeline(Program program);
        Program const& getProgram() const override;

    private:
        Program program_;
    };
} // namespace ase

