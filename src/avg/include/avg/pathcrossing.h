#pragma once

#include "path.h"
#include "vector.h"
#include "avgvisibility.h"

#include <vector>
#include <cstddef>

namespace avg
{
    /**
     * @brief A single transversal crossing of an infinite line against a path.
     *
     * The crossing is reported in the path's resolved space (the path's
     * transform has been applied). Tangential touches, where the line grazes a
     * curve without passing through it, are not reported.
     */
    struct PathCrossing
    {
        /** @brief The intersection point in resolved path space. */
        Vector2f point;

        /**
         * @brief Signed distance of point from linePoint along lineDir.
         *
         * Positive values lie on the lineDir side of linePoint, so a caller can
         * treat the line as a ray by keeping crossings with lineParam greater
         * than zero.
         */
        float lineParam;

        /** @brief Parameter within the crossed segment, in the range [0, 1). */
        float segmentT;

        /** @brief Index of the crossed segment in the walked segment sequence. */
        std::size_t segmentIndex;

        /**
         * @brief Sign of the path tangent crossed with lineDir, either +1 or -1.
         *
         * Summing these over one-sided crossings gives the winding number; the
         * even-odd rule is the parity of the crossing count.
         */
        int windingSign;
    };

    /**
     * @brief Computes every crossing of an infinite line against a path.
     *
     * The line passes through linePoint with direction lineDir (which need not
     * be a unit vector). Each segment is intersected analytically, without
     * flattening to line segments: lines give a linear equation, conics a
     * quadratic, cubics a cubic, and arcs a line-circle intersection.
     *
     * Every start-delimited subpath is treated as implicitly closed, matching
     * fill semantics, so an open subpath gains a closing edge from its last
     * point back to its start. Crossings at a vertex shared by two segments are
     * reported once, and tangential touches are not reported.
     *
     * A degenerate lineDir of zero length yields no crossings.
     */
    AVG_EXPORT std::vector<PathCrossing> pathLineCrossings(
            Path const& path,
            Vector2f linePoint,
            Vector2f lineDir);
}
