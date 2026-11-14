#pragma once

#include "avgvisibility.h"

#include <avg/drawing.h>
#include <avg/painter.h>
#include <avg/targetimage.h>

#include <ase/commandbuffer.h>

#include <pmr/memory_resource.h>

namespace avg
{
    using RenderElement = avg::Drawing::Element;

    /**
     * @brief Records the draws that paint drawing over target into queue.
     *
     * drawing is in logical units, scalingFactor of target's pixels each,
     * and must be flushed: its pending shadows are not drawn.
     */
    AVG_EXPORT void render(
            pmr::memory_resource* memory,
            ase::CommandBuffer& queue,
            ase::RenderContext& context,
            TargetImage& target,
            float scalingFactor,
            avg::Painter& painter,
            avg::Drawing const& drawing);
}

