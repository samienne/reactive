#pragma once

#include "map.h"
#include "signal.h"

#include <btl/all.h>

namespace reactive::signal
{
    template <typename T, typename U, typename V, typename = std::enable_if_t<
            std::is_convertible_v<SignalType<Signal<V, U>>, T>
        >>
    auto cast(Signal<V, U> sig)
    {
        return map([](SignalType<Signal<V, U>> u) -> T
                {
                    return std::forward<decltype(u)>(u);
                }, std::move(sig));
    }
} // reactive::signal

