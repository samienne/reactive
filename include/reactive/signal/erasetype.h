#pragma once

#include "reactive/signal.h"
#include "reactive/reactivevisibility.h"

namespace reactive::signal
{
    template <typename T, typename U>
    auto eraseType(Signal<T, U> sig) -> Signal<T, void>
    {
        return std::move(sig);
    }
}

