#pragma once

#include "typed.h"
#include "reactive/signal2.h"
//#include "reactive/signaltype.h"
#include "reactive/signaltraits.h"

#include <btl/shared.h>

namespace reactive::signal
{

    //static_assert(IsSignal<Share<signal::Typed<signal::Constant<int>, int>>>::value, "");
    //static_assert(IsSignal<Share<SignalBase<int>>>::value, "");

    /*
    template <typename TSignal, typename = std::enable_if_t<
        IsSignal<TSignal>::value
        >>
    Share<std::decay_t<TSignal>> share(TSignal signal)
    {
        return { std::move(signal) };
    }

    template <typename T>
    auto share(Share<T> sig) -> Share<T>
    {
        return std::move(sig);
    }

    template <typename T>
    auto cache(Share<T> sig) -> Share<T>
    {
        return std::move(sig);
    }
    */

    template <typename T, typename U>
    auto share(signal2::Signal<T, U> sig)
    {
        return signal2::SharedSignal<T, U>::create(
                Share<signal::Typed<U, T>>(
                    std::make_shared<signal::Typed<U, T>>(std::move(sig))
                    )
                );
    }

    template <typename T, typename U>
    auto share(signal2::Signal<T, Share<U>> sig)
    {
        return std::move(sig);
    }

    template <typename T>
    auto share(signal2::Signal<T, void> sig)
    {
        return std::move(sig);
    }

    template <typename T, typename U>
    auto share(signal2::SharedSignal<T, U> sig)
    {
        return std::move(sig);
    }

    /*
    template <typename T>
    auto share(signal2::Signal<T> sig) -> signal2::Signal<T>
    {
        return std::move(sig);
    }
    */
} // reactive::signal

