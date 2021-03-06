#pragma once

#include "share.h"
#include "map.h"
#include "signal.h"

#include <btl/tuplemap.h>

#include <utility>

namespace reactive::signal
{
    namespace detail
    {
        template <size_t i>
        struct Splitter
        {
            template <typename... T>
            auto operator()(std::tuple<T...> const& t) const
            -> std::decay_t<decltype(std::get<i>(t))>
            {
                return std::get<i>(t);
            }
        };

        template <typename TSignal, size_t... ns>
        auto splitImpl(TSignal&& sig, std::index_sequence<ns...>)
        {
            auto shared = share(std::forward<TSignal>(sig));
            return std::make_tuple(
                    signal::map(
                        Splitter<ns>(),
                        shared
                        )...
                    );
        }
    } // detail

    template <typename TSignal>
    auto split(TSignal&& sig)
    /*-> decltype(
        detail::splitImpl(std::forward<TSignal>(sig),
                std::make_index_sequence<
                    std::tuple_size<std::decay_t<SignalType<TSignal>>>::value
                >())
        )
        */
    {
        return detail::splitImpl(std::forward<TSignal>(sig),
                std::make_index_sequence<
                    std::tuple_size<std::decay_t<SignalType<TSignal>>>::value
                >());
    }
} // namespace reactive::signal

