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

} // namespace ase

