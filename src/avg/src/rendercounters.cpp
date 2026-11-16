#include "rendercounters.h"

namespace avg
{

bool RenderCounters::operator==(RenderCounters const& rhs) const
{
    return passes == rhs.passes
        && draws == rhs.draws
        && snapshots == rhs.snapshots
        && scratchTargets == rhs.scratchTargets
        && scratchBytes == rhs.scratchBytes
        && pixelsShaded == rhs.pixelsShaded;
}

bool RenderCounters::operator!=(RenderCounters const& rhs) const
{
    return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, RenderCounters const& counters)
{
    return stream << "RenderCounters{passes " << counters.passes
        << ", draws " << counters.draws
        << ", snapshots " << counters.snapshots
        << ", scratchTargets " << counters.scratchTargets
        << ", scratchBytes " << counters.scratchBytes
        << ", pixelsShaded " << counters.pixelsShaded << "}";
}

} // namespace avg
