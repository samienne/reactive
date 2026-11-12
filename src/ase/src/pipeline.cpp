#include "pipeline.h"

#include "framebuffer.h"
#include "rendercontext.h"
#include "platform.h"

namespace ase
{

Pipeline::Pipeline(std::shared_ptr<PipelineImpl> impl) :
    deferred_(std::move(impl))
{
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

VertexSpec const& Pipeline::getVertexSpec() const
{
    return d()->getVertexSpec();
}

Format Pipeline::getColorFormat() const
{
    return d()->getColorFormat();
}

std::optional<Format> Pipeline::getDepthFormat() const
{
    return d()->getDepthFormat();
}

bool Pipeline::isBlendEnabled() const
{
    return d()->isBlendEnabled();
}

bool Pipeline::isDepthTestEnabled() const
{
    return d()->isDepthTestEnabled();
}

bool Pipeline::isDepthWriteEnabled() const
{
    return d()->isDepthWriteEnabled();
}

bool Pipeline::matchesTarget(Framebuffer const& target) const
{
    if (target.getColorFormat() != getColorFormat())
        return false;

    std::optional<Format> const targetDepth = target.getDepthFormat();
    std::optional<Format> const depth = getDepthFormat();
    if (targetDepth.has_value() != depth.has_value())
        return false;

    return !depth || *targetDepth == FORMAT_UNKNOWN || *targetDepth == *depth;
}

} // namespace ase

