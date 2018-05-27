#pragma once

#include "vector.h"

#include <avg/transform.h>

#include <btl/visibility.h>

#include <ostream>

namespace ase
{
    enum class ButtonState
    {
        down,
        up
    };

    struct BTL_VISIBLE PointerButtonEvent
    {
        unsigned int pointer;
        unsigned int button;
        ButtonState state;
        Vector2f pos;
    };

    BTL_VISIBLE std::ostream& operator<<(std::ostream& stream,
            PointerButtonEvent const& e);
}

