#pragma once

#include "reactive/signal.h"
#include "reactive/reactivevisibility.h"

namespace reactive::signal
{
    template <typename T, typename U>
    auto eraseType(Signal<U, T> sig) -> AnySignal<T>
    {
        return std::move(sig);
    }
}

