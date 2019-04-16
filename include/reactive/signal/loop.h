#pragma once

#include "map.h"
#include "reactive/reactivevisibility.h"

namespace reactive::signal
{

    namespace detail
    {
        template <typename T>
        struct modulo
        {
            T operator()(T const& v, T const& m) const
            {
                auto r = v;
                while (m < r)
                    r = r - m;
                return r;
            }
        };
    } // detail

    template <typename TSignal, typename T, typename = typename std::enable_if
        <
            IsSignal<TSignal>::value &&
            std::is_same<typename SignalValueType<TSignal>::type, T>::value
        >::type>
    auto loop(TSignal&& sig, T&& max)
    -> decltype(signal::map(detail::modulo<T>(), std::forward<TSignal>(sig),
                constant(std::forward<T>(max))))
    //-> Signal<T>
    {
        return signal::map(detail::modulo<T>(), std::forward<TSignal>(sig),
                constant(std::forward<T>(max)));
    }
} // reactive::signal

