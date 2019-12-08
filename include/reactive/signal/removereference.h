#pragma once

#include "map.h"

#include "signal.h"

#include <btl/hidden.h>

namespace reactive::signal
{
    template <typename T, typename U>
    auto removeReference(Signal<U, T> sig)
    {
        return signal::map([](auto&& v) -> std::decay_t<T>
        {
            return std::forward<decltype(v)>(v);
        },
        std::move(sig));
    }
} // reactive::signal

