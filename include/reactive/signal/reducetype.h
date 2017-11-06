#pragma once

#include "reactive/signal.h"

namespace reactive::signal
{
    template <typename T, typename U>
    auto reduceType(Signal<T, U> sig) -> Signal<T, void>
    {
        return std::move(sig);
    }
}

