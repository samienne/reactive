#pragma once

#include "map.h"
#include <reactive/signal.h>

#include <btl/all.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive::signal
{
    template <typename T, typename U, typename V, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<SignalType<Signal<U, V>>, T>
        >::value
        >>
    auto cast(Signal<U, V> sig)
    {
        return map([](SignalType<Signal<U, V>> u) -> T
                {
                    return std::forward<decltype(u)>(u);
                }, std::move(sig));
    }
} // reactive::signal

BTL_VISIBILITY_POP

