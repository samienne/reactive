#pragma once

#include "map.h"
#include <reactive/signal2.h>

#include <btl/all.h>

namespace reactive::signal
{
    template <typename T, typename U, typename V, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<SignalType<signal2::Signal<U, V>>, T>
        >::value
        >>
    auto cast(signal2::Signal<U, V> sig)
    {
        return map([](SignalType<signal2::Signal<U, V>> u) -> T
                {
                    return std::forward<decltype(u)>(u);
                }, std::move(sig));
    }
} // reactive::signal

