#pragma once

#include "constant.h"

namespace reactive::signal
{
    auto false_()
    {
        return signal::constant(false);
    }
} // namespace reactive::signal

