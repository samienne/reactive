#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <array>

namespace ase
{
    struct ASE_EXPORT PointerMoveEvent
    {
        Vector2f pos;
        Vector2f rel;
        std::array<bool, 15> buttons;
        bool hover;
    };

    ASE_EXPORT bool isButtonDown(PointerMoveEvent const& e);
} // ase

