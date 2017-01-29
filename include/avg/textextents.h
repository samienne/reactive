#pragma once

#include <ase/vector.h>

#include <ostream>

namespace avg
{
    struct TextExtents
    {
        TextExtents()
        {
        }

        TextExtents(ase::Vector2f bearing, ase::Vector2f size,
                ase::Vector2f advance) :
            bearing(bearing),
            size(size),
            advance(advance)
        {
        }

        // The top-left corner from the baseline origin
        ase::Vector2f bearing = ase::Vector2f(0.0f, 0.0f);

        // The size
        ase::Vector2f size = ase::Vector2f(0.0f, 0.0f);

        // The next position after these characters
        ase::Vector2f advance = ase::Vector2f(0.0f, 0.0f);
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

