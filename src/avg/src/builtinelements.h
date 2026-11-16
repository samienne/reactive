#pragma once

#include <avg/drawing.h>
#include <avg/avgvisibility.h>

#include <pmr/memory_resource.h>

namespace avg
{
    // The effect elements avg uses itself. Their sizes are logical units,
    // converted to pixels when the frame is rendered. None of their Region
    // passes reads the region it covers, so equal passes can merge.

    // content composited at alpha, which must be within [0, 1].
    AVG_EXPORT Drawing groupOpacity(pmr::memory_resource* memory,
            Drawing content, float alpha);

    // content blurred by a Gaussian reaching radius, which must not be
    // negative: a downsample, a horizontal and a vertical blur at
    // frostedGlassDivisor(radius), and a composite, nested in that order.
    // A radius of zero returns content as it is.
    AVG_EXPORT Drawing contentBlur(pmr::memory_resource* memory,
            Drawing content, float radius);
}
