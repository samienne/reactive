#pragma once

#include "vector.h"

#include <btl/visibility.h>

#include <array>

namespace ase
{
    struct BTL_VISIBLE PointerMoveEvent
    {
        Vector2f pos;
        Vector2f rel;
        std::array<bool, 15> buttons;
        bool hover;
    };

    BTL_VISIBLE bool isButtonDown(PointerMoveEvent const& e);
} // ase

