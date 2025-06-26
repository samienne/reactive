#pragma once

#include "vector.h"

#include "asevisibility.h"

#include <ostream>

namespace ase
{
    enum class ButtonState
    {
        down,
        up
    };

    struct ASE_EXPORT PointerButtonEvent
    {
        unsigned int pointer;
        unsigned int button;
        ButtonState state;
        Vector2f pos;
    };

    ASE_EXPORT std::ostream& operator<<(std::ostream& stream,
            PointerButtonEvent const& e);
}

