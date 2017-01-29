#pragma once

#include <stdint.h>

namespace ase
{
    enum class KeyModifier : uint32_t
    {
        Empty = 0x0,
        Shift = 0x1,
        Control = 0x2,
        Alt = 0x4,
        Meta = 0x8,
        Keypad = 0x16
    };

} // ase

