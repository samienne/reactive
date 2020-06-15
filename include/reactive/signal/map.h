#pragma once

#include "signal.h"
#include "signaltraits.h"
#include "group.h"

namespace reactive::signal
{
    template <typename TFunc, typename... Ts, typename... Us>
    auto map(TFunc&& func, Signal<Us, Ts>... s) -> decltype(auto)
    {
        return group(std::move(s)...).map(std::forward<TFunc>(func));
    }

    template <typename TFunc, typename... TSigs,
             typename = typename
        std::enable_if
        <
            btl::All<
                std::is_copy_constructible<std::decay_t<TFunc>>,
                IsSignal<TSigs>...
            >::value
        >::type>
    constexpr auto mapFunction(TFunc&& func, TSigs... sigs)
    {
        return group(std::move(sigs)...).mapToFunction(std::forward<TFunc>(func));
    }
} // reactive::signal

namespace btl
{
    template <typename... Ts>
    auto fmap(Ts&&... ts)
    -> decltype(
            reactive::signal::map(std::forward<Ts>(ts)...)
        )
    {
        return reactive::signal::map(std::forward<Ts>(ts)...);
    }
} // btl

