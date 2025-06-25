#pragma once

#include <bq/signal/signal.h>

#include <avg/animated.h>

namespace bqui
{

template <typename T, typename U>
auto animate(bq::signal::Signal<T, U> sig)
{
    return std::move(sig).map([](auto value)
            {
                return avg::Animated<std::decay_t<decltype(value)>>(std::move(value));
            });
}

}

