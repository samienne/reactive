#pragma once

#include "map.h"
#include "join.h"
#include "signal.h"

#include <btl/mbind.h>

namespace reactive::signal
{
    template <typename TFunc, typename... Ts, typename... Us>
    auto mbind(TFunc&& func, Signal<Us, Ts>... ts)
    -> decltype(
            signal::join(signal::map(std::forward<TFunc>(func),
                    std::move(ts)...))
            )
    {
        return signal::join(signal::map(std::forward<TFunc>(func),
                std::move(ts)...));
    }
} // namespace reactive::signal

namespace btl
{
    template <typename TFunc, typename... Ts, typename... Us>
    auto mbind(TFunc&& func, reactive::Signal<Us, Ts>... ts)
    -> decltype(reactive::signal::mbind(
                std::forward<TFunc>(func),
                std::move(ts)...
                )
            )
    {
        return reactive::signal::mbind(
                std::forward<TFunc>(func),
                std::move(ts)...
                );
    }
} // namespace btl

