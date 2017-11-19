#pragma once

#include "vector.h"

#include <ostream>

namespace avg
{
    struct TextExtents
    {
        inline TextExtents()
        {
        }

        inline TextExtents(Vector2f bearing, Vector2f size, Vector2f advance) :
            bearing(bearing),
            size(size),
            advance(advance)
        {
        }

        // The top-left corner from the baseline origin
        Vector2f bearing = Vector2f(0.0f, 0.0f);

        // The size
        Vector2f size = Vector2f(0.0f, 0.0f);

        // The next position after these characters
        Vector2f advance = Vector2f(0.0f, 0.0f);
    };

    inline std::ostream& operator<<(std::ostream& stream, TextExtents const& te)
    {
        return stream
            << "TextExtents{"
            << "bearing:{" << te.bearing[0] << "," << te.bearing[1] << "}, "
            << "size:{" << te.size[0] << "," << te.size[1] << "}, "
            << "advance:{" << te.advance[0] << "," << te.advance[1] << "}"
            << "}";
    }
}

