#pragma once

#include "typed.h"
#include "cache.h"
#include "reactive/signal2.h"
//#include "reactive/signaltype.h"
#include "reactive/signaltraits.h"

#include <btl/shared.h>

namespace reactive::signal
{
    namespace detail
    {
        template <typename T, typename U>
        auto makeShared(signal2::Signal<T, U> sig)
        {
            return signal2::SharedSignal<T, U>::create(
                    Share<T, signal::Typed<T, U>>(
                        std::make_shared<signal::Typed<T, U>>(
                            std::move(sig).signal()
                            )
                        )
                    );
        }
    } // detail

    template <typename T, typename U>
    auto share(signal2::Signal<T, U> sig)
    {
        return detail::makeShared(cache(std::move(sig)));
    }

    template <typename T, typename U>
    auto share(signal2::Signal<T, Share<T, U>> sig)
    {
        return std::move(sig);
    }

    template <typename T>
    auto share(signal2::Signal<T, void> sig)
    {
        return signal2::SharedSignal<T, void>::create(
                std::move(sig)
                );
    }

    template <typename T, typename U>
    auto share(signal2::SharedSignal<T, U> sig)
    {
        return std::move(sig);
    }
} // reactive::signal

