#pragma once

#include "reactive/signal2.h"

namespace reactive::signal
{
    template <typename T, typename U>
    auto reduceType(signal2::Signal<T, U> sig) -> signal2::Signal<T, void>
    {
        return std::move(sig);
    }
}

