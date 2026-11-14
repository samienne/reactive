#include "glplatform.h"

#include <ase/pipelineimpl.h>
#include <ase/pipelinedesc.h>
#include <ase/dispatcher.h>

#include <btl/visibility.h>

#include "systemgl.h"

namespace ase
{
    class GlRenderContext;
    struct GlFunctions;

    class GlPipeline : public PipelineImpl
    {
    public:
        GlPipeline(
                GlRenderContext& context,
                Program program,
                VertexSpec vertexSpec,
                PipelineDesc desc);

        GlPipeline(GlPipeline const& rhs) = default;
        GlPipeline(GlPipeline&& rhs) noexcept = default;

        GlPipeline& operator=(GlPipeline const& rhs) = default;
        GlPipeline& operator=(GlPipeline&& rhs) noexcept = default;

        bool operator==(GlPipeline const& rhs) const;
        bool operator!=(GlPipeline const& rhs) const;
        bool operator<(GlPipeline const& rhs) const;

        virtual Program const& getProgram() const override;
        VertexSpec const& getVertexSpec() const override;
        Format getColorFormat() const override;
        std::optional<Format> getDepthFormat() const override;
        bool isBlendEnabled() const override;
        bool isDepthTestEnabled() const override;
        bool isDepthWriteEnabled() const override;
        BlendMode getSrcFactor() const;
        BlendMode getDstFactor() const;

    private:
        Program program_;
        VertexSpec vertexSpec_;
        Format colorFormat_;
        std::optional<Format> depthFormat_;
        BlendMode srcFactor_;
        BlendMode dstFactor_;
        bool blendEnabled_;
        bool depthTest_;
        bool depthWrite_;
    };
} // namespace ase

