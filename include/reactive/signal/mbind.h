#pragma once

#include "map.h"
#include "join.h"

#include <btl/mbind.h>

namespace reactive
{
    namespace signal
    {
        template <typename TFunc, typename... Ts>
        auto mbind(TFunc&& func, Ts&&... ts)
        -> decltype(
                signal::join(signal::map(std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...))
                )
        {
            return signal::join(signal::map(std::forward<TFunc>(func),
                    std::forward<Ts>(ts)...));
        }
    } // signal
} // reactive

namespace btl
{
    template <typename TFunc, typename... Ts>
    auto mbind(TFunc&& func, Ts&&... ts)
    -> decltype(reactive::signal::mbind(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                )
            )
    {
        return reactive::signal::mbind(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }
} // btl

