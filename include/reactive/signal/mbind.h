#pragma once

#include "map.h"
#include "join.h"
#include "reactive/signal2.h"

#include <btl/mbind.h>

namespace reactive
{
    namespace signal
    {
        template <typename TFunc, typename... Ts, typename... Us>
        auto mbind(TFunc&& func, signal2::Signal<Ts, Us>... ts)
        -> decltype(
                signal::join(signal::map(std::forward<TFunc>(func),
                        std::move(ts)...))
                )
        {
            return signal::join(signal::map(std::forward<TFunc>(func),
                    std::move(ts)...));
        }
    } // signal
} // reactive

namespace btl
{
    template <typename TFunc, typename... Ts, typename... Us>
    auto mbind(TFunc&& func, reactive::signal2::Signal<Ts, Us>... ts)
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
} // btl

