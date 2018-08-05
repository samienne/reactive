#pragma once

#include "constant.h"

namespace reactive::signal
{
    auto true_()
    {
        return signal::constant(true);
    }
} // namespace reactive::signal

