#pragma once

#include "cast.h"
#include "reactive/signal.h"
#include "reactive/reactivevisibility.h"

namespace reactive::signal
{
    template <typename T>
    class Convert;

    template <typename T>
    struct IsSignal<Convert<T>> : std::true_type {};

    template <typename T>
    class Convert : public AnySignal<T>
    {
    public:
        template <typename U, typename V, typename = std::enable_if_t<
            std::is_convertible<U, T>::value
            >>
        Convert(Signal<V, U> sig) :
            AnySignal<T>(cast<T>(std::move(sig)))
        {
        }

        Convert(Convert&&) = default;
        Convert& operator=(Convert&&) = default;

    private:
        Convert(Convert const&) = default;
        Convert& operator=(Convert const&) = default;
    };

} // reactive::signal

