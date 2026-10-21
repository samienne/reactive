#pragma once

#include "vector.h"
#include "avgvisibility.h"

#include <cstdint>
#include <ostream>

namespace avg
{
    struct AVG_EXPORT PositionedGlyph
    {
        // The font's glyph index, as FreeType numbers them
        uint32_t glyphIndex = 0;

        // The pen origin on the baseline, y up, in the same units as the
        // path textToPath draws
        Vector2f position = Vector2f(0.0f, 0.0f);
    };

    inline std::ostream& operator<<(std::ostream& stream,
            PositionedGlyph const& g)
    {
        return stream
            << "PositionedGlyph{"
            << "glyph:" << g.glyphIndex << ", "
            << "position:{" << g.position[0] << "," << g.position[1] << "}"
            << "}";
    }
}
