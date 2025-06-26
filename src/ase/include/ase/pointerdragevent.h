#pragma once

#include "vector.h"

#include <btl/visibility.h>

namespace ase
{
    struct PointerDragEvent
    {
        int button;
        Vector2f pos;
        Vector2f rel;
        Vector2f orig;
        bool hover;
    };
} // namespace ase
