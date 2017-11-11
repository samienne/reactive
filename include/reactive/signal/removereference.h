#pragma once

#include "map.h"

#include <reactive/signal.h>

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

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

BTL_VISIBILITY_POP

