#pragma once

#include "reactive/signal/map.h"
#include "signal.h"

namespace reactive::signal
{
    template <typename T, typename U>
    auto toString(Signal<T, U> s)
    {
        return signal::map([](auto&& v)
                {
                    return std::to_string(std::forward<decltype(v)>(v));
                }, std::move(s));
    }
} // namespace reactive::signal

