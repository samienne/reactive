#pragma once

#include "collect.h"
#include "stream.h"

#include <btl/copywrapper.h>

#include <type_traits>

namespace bq
{
    namespace stream
    {
        template <typename TFunc, typename TInitial, typename T,
                    typename... TSignals>
        class Iterate;

        template <typename TFunc, typename TInitial, typename T,
                    typename... TSignals>
        class IterateStatic;
    }
}

namespace bq::stream
{
    template <typename TFunc, typename T, typename U, typename V, typename... TSignals>
    auto iterate(TFunc&& func, signal::Signal<U, V> initial,
            Stream<T> stream, TSignals&&... sigs)
    {
        return merge(
                collect(std::move(stream)),
                initial.withChanged(true),
                std::forward<TSignals>(sigs)...)
            .template withPrevious<std::optional<V>>(
                    [func=btl::copyWrap(std::forward<TFunc>(func))](
                        std::optional<V> const& previous,
                        std::vector<T> const& streamValues,
                        bool initialChanged,
                        V const& initial,
                        auto&&... values) -> std::optional<V>
                {
                    V current = (initialChanged || !previous) ?
                        initial : *previous;

                    for (auto&& v : streamValues)
                        current = (*func)(std::move(current), v, values...);

                    return current;
                },
                std::nullopt
                )
            .map([](std::optional<V> const& value)
                {
                    return *value;
                });

    }

    template <typename TFunc, typename T, typename U, typename... TSignals,
             typename = std::enable_if_t<
                 !signal::IsSignal<std::decay_t<U>>::value
             >>
    auto iterate(TFunc&& func, U&& initial, Stream<T> stream, TSignals&&... sigs)
    {
        return iterate(std::forward<TFunc>(func),
                signal::constant(std::forward<U>(initial)),
                std::move(stream),
                std::forward<TSignals>(sigs)...);
    }
} // namespace reactive::stream

