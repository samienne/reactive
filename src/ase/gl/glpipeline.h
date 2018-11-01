#include "pipelineimpl.h"

#include <btl/visibility.h>

namespace ase
{
    class GlPipeline : public PipelineImpl
    {
    public:
        GlPipeline(Program program,
                VertexSpec vertexSpec);

        GlPipeline(Program program,
                VertexSpec vertexSpec,
                BlendMode srcFactor,
                BlendMode dstFactor);

        GlPipeline(GlPipeline const& rhs) = default;
        GlPipeline(GlPipeline&& rhs) noexcept = default;

        GlPipeline& operator=(GlPipeline const& rhs) = default;
        GlPipeline& operator=(GlPipeline&& rhs) noexcept = default;

        bool operator==(GlPipeline const& rhs) const;
        bool operator!=(GlPipeline const& rhs) const;
        bool operator<(GlPipeline const& rhs) const;

        virtual Program const& getProgram() const override;
        VertexSpec const& getVertexSpec() const;
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
} // namespace ase

