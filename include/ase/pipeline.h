#pragma once

#include "blendmode.h"
#include "program.h"
#include "vertexspec.h"

namespace ase
{
    class NamedVertexSpec;
    class Program;
    class RenderContext;

    class Pipeline
    {
    public:
        Pipeline(RenderContext& context, Program program,
                VertexSpec vertexSpec);
        Pipeline(RenderContext& context, Program program,
                VertexSpec vertexSpec,
                BlendMode srcFactor,
                BlendMode dstFactor);
        Pipeline(Pipeline const& rhs) = default;
        Pipeline(Pipeline&& rhs) = default;

        Pipeline& operator=(Pipeline const& rhs) = default;
        Pipeline& operator=(Pipeline&& rhs) = default;

        bool operator==(Pipeline const& rhs) const;
        bool operator!=(Pipeline const& rhs) const;
        bool operator<(Pipeline const& rhs) const;

        Program const& getProgram() const;
        VertexSpec const& getSpec() const;
        bool isBlendEnabled() const;
        BlendMode getSrcFactor() const;
        BlendMode getDstFactor() const;

    private:
        Program program_;
        VertexSpec vertexSpec_;
        BlendMode srcFactor_;
        BlendMode dstFactor_;
        bool blendEnabled_;
    };
}

