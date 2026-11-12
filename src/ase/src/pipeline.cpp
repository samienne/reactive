#include "pipeline.h"

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

} // namespace ase

