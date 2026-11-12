#pragma once

#include "pipelineimpl.h"

namespace ase
{
    class DummyPipeline : public PipelineImpl
    {
    public:
        DummyPipeline(Program program, VertexSpec vertexSpec,
                bool blendEnabled);
        Program const& getProgram() const override;
        VertexSpec const& getVertexSpec() const override;
        bool isBlendEnabled() const override;

    private:
        Program program_;
        VertexSpec vertexSpec_;
        bool blendEnabled_;
    };
} // namespace ase
