#pragma once

#include "pipelineimpl.h"

namespace ase
{
    class DummyPipeline : public PipelineImpl
    {
    public:
        DummyPipeline(Program program, bool blendEnabled);
        Program const& getProgram() const override;
        bool isBlendEnabled() const override;

    private:
        Program program_;
        bool blendEnabled_;
    };
} // namespace ase

