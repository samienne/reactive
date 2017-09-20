#pragma once

#include "map.h"

#include <reactive/signal2.h>

namespace reactive::signal
{
    template <typename T, typename U>
    auto removeReference(signal2::Signal<T, U> sig)
    {
        return signal::map([](auto&& v) -> std::decay_t<T>
        {
            return std::forward<decltype(v)>(v);
        },
        std::move(sig));
    }
} // reactive::signal

