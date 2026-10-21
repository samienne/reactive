#pragma once

#include "avgvisibility.h"

#include <cstdint>
#include <ostream>
#include <vector>

namespace avg
{
    struct AVG_EXPORT GlyphBitmap
    {
        // Size in pixels; either may be zero for a blank glyph
        int width = 0;
        int height = 0;

        // Pixels from the pen origin right to the bitmap's left column
        int bearingX = 0;

        // Pixels from the baseline up to the bitmap's top row
        int bearingY = 0;

        // One byte per pixel, 0 to 255, rows top to bottom and width
        // pixels each
        std::vector<uint8_t> coverage;
    };

    inline std::ostream& operator<<(std::ostream& stream,
            GlyphBitmap const& b)
    {
        return stream
            << "GlyphBitmap{"
            << "size:{" << b.width << "," << b.height << "}, "
            << "bearing:{" << b.bearingX << "," << b.bearingY << "}"
            << "}";
    }
}
