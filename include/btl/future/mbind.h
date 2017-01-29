#pragma once

#include "fmap.h"
#include "join.h"

#include <btl/mbind.h>

namespace btl
{
    namespace future
    {
        template <typename TFunc, typename... TFutures,
                 typename = std::enable_if_t<
                     All<IsFuture<TFutures>...>::value
                     >
                >
        auto mbind(TFunc&& func, TFutures&&... futures)
        -> decltype(
            join(
                fmap(std::forward<TFunc>(func),
                    std::forward<TFutures>(futures)...
                    )
                )
            )
        {
            return join(fmap(std::forward<TFunc>(func),
                        std::forward<TFutures>(futures)...));
        }
    } // future

    template <typename... Ts>
    auto mbind(Ts&&... ts)
    -> decltype(
            future::mbind(std::forward<Ts>(ts)...)
        )
    {
        return future::mbind(std::forward<Ts>(ts)...);
    }
} // btl

