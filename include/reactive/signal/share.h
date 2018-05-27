#pragma once

#include "typed.h"
#include "cache.h"
#include "reactive/signal.h"
#include "reactive/signaltraits.h"

#include <btl/shared.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive::signal
{
    namespace detail
    {
        template <typename T, typename U>
        auto makeShared(Signal<T, U> sig)
        {
            return SharedSignal<T, U>::create(
                    Share<T, signal::Typed<T, U>>(
                        std::make_shared<signal::Typed<T, U>>(
                            std::move(sig).signal()
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
    auto share(Signal<T, Share<T, U>> sig)
    {
        return std::move(sig);
    }

    template <typename T>
    auto share(Signal<T, void> sig)
    {
        return SharedSignal<T, void>::create(
                std::move(sig)
                );
    }

    template <typename T, typename U>
    auto share(SharedSignal<T, U> sig)
    {
        return std::move(sig);
    }
} // reactive::signal

BTL_VISIBILITY_POP

