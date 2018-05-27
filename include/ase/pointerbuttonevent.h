#pragma once

#include "vector.h"

#include <avg/transform.h>

#include <ostream>

namespace ase
{
    enum class ButtonState
    {
        down,
        up
    };

    struct PointerButtonEvent
    {
        unsigned int pointer;
        unsigned int button;
        ButtonState state;
        Vector2f pos;
    };

    std::ostream& operator<<(std::ostream& stream, PointerButtonEvent const& e);
}

