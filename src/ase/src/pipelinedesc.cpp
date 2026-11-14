#include "pipelinedesc.h"

namespace ase
{

bool PipelineDesc::writesDepth() const
{
    return depthWrite.value_or(!blend.has_value());
}

} // namespace ase
