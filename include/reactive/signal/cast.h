#pragma once

#include <reactive/signal2.h>

#include <btl/all.h>

namespace reactive::signal
{
    template <typename T, typename U, typename V, typename std::enable_if_t<
        btl::All<
            std::is_convertible<U, T>
        >::value
        >>
    auto cast(signal2::Signal<U, V> sig)
    {
        return signal2::Signal<T, V>(std::move(sig));
    }
} // reactive::signal

