#pragma once

#include "signal.h"

namespace reactive::signal
{
    template <typename T, typename U>
    auto eraseType(Signal<U, T> sig) -> AnySignal<T>
    {
        return sig;
    }
}

