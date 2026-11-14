#include "pipelinedesc.h"

namespace ase
{

PipelineDesc::PipelineDesc(Format colorFormat,
        std::optional<Format> depthFormat) :
    colorFormat(colorFormat),
    depthFormat(depthFormat)
{
}

bool PipelineDesc::writesDepth() const
{
    return depthWrite.value_or(!blend.has_value());
}

} // namespace ase
