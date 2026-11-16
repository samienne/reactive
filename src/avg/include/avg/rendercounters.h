#pragma once

#include "avgvisibility.h"

#include <cstddef>
#include <ostream>

namespace avg
{
    /**
     * @brief What one render recorded, as totals over the frame.
     *
     *     RenderCounters counters = painter.paintToImage(target, 1.0f,
     *             drawing);
     *
     * draws counts every draw command, content and effect alike. passes
     * counts effect passes, the final one included; snapshots counts the
     * backdrop copies, which are draws but not passes. pixelsShaded sums
     * the region of every pass and snapshot draw in its own target's
     * pixels; content draws add nothing to it. scratchTargets and
     * scratchBytes are the scratch textures the frame's plan uses and
     * their memory.
     */
    struct AVG_EXPORT RenderCounters
    {
        size_t passes = 0;
        size_t draws = 0;
        size_t snapshots = 0;
        size_t scratchTargets = 0;
        size_t scratchBytes = 0;
        size_t pixelsShaded = 0;

        bool operator==(RenderCounters const& rhs) const;
        bool operator!=(RenderCounters const& rhs) const;
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream,
            RenderCounters const& counters);
}
