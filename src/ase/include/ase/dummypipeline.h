#pragma once

#include "pipelineimpl.h"
#include "pipelinedesc.h"

namespace ase
{
    class DummyPipeline : public PipelineImpl
    {
    public:
        DummyPipeline(Program program, VertexSpec vertexSpec,
                PipelineDesc desc);
        Program const& getProgram() const override;
        VertexSpec const& getVertexSpec() const override;
        Format getColorFormat() const override;
        std::optional<Format> getDepthFormat() const override;
        bool isBlendEnabled() const override;
        bool isDepthTestEnabled() const override;
        bool isDepthWriteEnabled() const override;

    private:
        Program program_;
        VertexSpec vertexSpec_;
        PipelineDesc desc_;
    };
} // namespace ase
