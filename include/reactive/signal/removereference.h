#pragma once

#include "map.h"

#include <reactive/signal.h>

namespace reactive::signal
{
    template <typename T, typename U>
    auto removeReference(Signal<T, U> sig)
    {
        return signal::map([](auto&& v) -> std::decay_t<T>
        {
            return std::forward<decltype(v)>(v);
        },
        std::move(sig));
    }
} // reactive::signal

