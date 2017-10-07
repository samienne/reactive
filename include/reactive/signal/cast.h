#pragma once

#include "map.h"
#include <reactive/signal2.h>

#include <btl/all.h>

namespace reactive::signal
{
    template <typename T, typename U, typename V, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<U, T>
        >::value
        >>
    auto cast(signal2::Signal<U, V> sig)
    {
        //return signal2::Signal<T, V>(signal2::wrap(std::move(sig).signal()));
        return map([](U u) -> T { return u; }, std::move(sig));
    }
} // reactive::signal

