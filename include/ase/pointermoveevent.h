#pragma once

#include "vector.h"

#include <array>

namespace ase
{
    struct PointerMoveEvent
    {
        int pointer;
        Vector2f pos;
        Vector2f rel;
        std::array<bool, 16> buttons;
    };

    bool isButtonDown(PointerMoveEvent const& e);
} // ase

