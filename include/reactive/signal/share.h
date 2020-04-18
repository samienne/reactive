#pragma once

#include "typed.h"
#include "cache.h"
#include "signal.h"
#include "signaltraits.h"

#include <btl/shared.h>

namespace reactive::signal
{
    namespace detail
    {
        template <typename T, typename U>
        auto makeShared(Signal<U, T> sig)
        {
            return SharedSignal<U, T>::create(
                    Share<Typed<U, T>, T>(
                        std::make_shared<Typed<U, T>>(
                            std::move(sig).storage()
                            )
                        )
                    );
        }
    } // detail

    template <typename T, typename... Us>
    auto share(Signal<T, Us...> sig)
    {
        return detail::makeShared(cache(std::move(sig)));
    }

    template <typename T, typename... Us>
    SharedSignal<Share<T, Us...>, Us...> share(Signal<Share<T, Us...>, Us...> sig)
    {
        return detail::makeShared(std::move(sig));
    }

    template <typename... Ts>
    auto share(Signal<void, Ts...> sig)
    {
        return SharedSignal<void, Ts...>::create(
                std::move(sig)
                );
    }

    template <typename T, typename... Us>
    auto share(SharedSignal<T, Us...> sig)
    {
        return sig;
    }
} // reactive::signal

