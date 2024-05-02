#pragma once

#include "signal2/signal.h"

#include <avg/animated.h>

namespace reactive
{

template <typename T, typename U>
auto animate(signal2::Signal<T, U> sig)
{
    return std::move(sig).map([](auto value)
            {
                return avg::Animated<std::decay_t<decltype(value)>>(std::move(value));
            });
}

} // namespace reactive

