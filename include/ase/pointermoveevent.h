#pragma once

#include "vector.h"

#include <array>

namespace ase
{
    struct PointerMoveEvent
    {
        Vector2f pos;
        Vector2f rel;
        std::array<bool, 15> buttons;
        bool hover;
    };

    bool isButtonDown(PointerMoveEvent const& e);
} // ase

