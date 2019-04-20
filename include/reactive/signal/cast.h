#pragma once

#include "map.h"
#include "reactive/signal.h"
#include "reactive/reactivevisibility.h"

#include <btl/all.h>

namespace reactive::signal
{
    template <typename T, typename U, typename V, typename = std::enable_if_t<
            std::is_convertible_v<SignalType<Signal<U, V>>, T>
        >>
    auto cast(Signal<U, V> sig)
    {
        return map([](SignalType<Signal<U, V>> u) -> T
                {
                    return std::forward<decltype(u)>(u);
                }, std::move(sig));
    }
} // reactive::signal

