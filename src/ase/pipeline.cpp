#include "pipeline.h"

#include "rendercontext.h"
#include "platform.h"

namespace ase
{

Pipeline::Pipeline(RenderContext& context,
        Program program,
        VertexSpec vertexSpec) :
    deferred_(context.getPlatform().makePipeline(
                context,
                std::move(program),
                std::move(vertexSpec)
                ))
{
    context.flush();
}

Pipeline::Pipeline(RenderContext& context,
        Program program,
        VertexSpec vertexSpec,
        BlendMode srcFactor,
        BlendMode dstFactor) :
    deferred_(context.getPlatform().makePipelineWithBlend(
                context,
                std::move(program),
                std::move(vertexSpec),
                srcFactor,
                dstFactor
                ))
{
    context.flush();
}

bool Pipeline::operator==(Pipeline const& rhs) const
{
    return d() == rhs.d();
}

bool Pipeline::operator!=(Pipeline const& rhs) const
{
    return d() != rhs.d();
}

bool Pipeline::operator<(Pipeline const& rhs) const
{
    return d() < rhs.d();
}

Program const& Pipeline::getProgram() const
{
    return d()->getProgram();
}

} // namespace ase

