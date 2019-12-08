#pragma once

#include "typed.h"
#include "cache.h"
#include "reactive/signal.h"
#include "reactive/signaltraits.h"
#include "reactive/reactivevisibility.h"

#include <btl/shared.h>

namespace reactive::signal
{
    namespace detail
    {
        template <typename T, typename U>
        auto makeShared(Signal<U, T> sig)
        {
            return SharedSignal<U, T>::create(
                    Share<signal::Typed<U, T>, T>(
                        std::make_shared<signal::Typed<U, T>>(
                            std::move(sig).storage()
                            )
                        )
                    );
        }
    } // detail

    template <typename T, typename U>
    auto share(Signal<T, U> sig)
    {
        return detail::makeShared(cache(std::move(sig)));
    }

    template <typename T, typename U>
    SharedSignal<Share<T, U>, U> share(Signal<Share<T, U>, U> sig)
    {
        return detail::makeShared(std::move(sig));
    }

    template <typename T>
    auto share(Signal<void, T> sig)
    {
        return SharedSignal<void, T>::create(
                std::move(sig)
                );
    }

    template <typename T, typename U>
    auto share(SharedSignal<T, U> sig)
    {
        return sig;
    }
} // reactive::signal

