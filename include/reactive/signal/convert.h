#pragma once

#include "cast.h"
#include "reactive/signal.h"

namespace reactive::signal
{
    template <typename T>
    class Convert : public Signal<T, void>
    {
    public:
        template <typename U, typename V, typename = std::enable_if_t<
            std::is_convertible<U, T>::value
            >>
        Convert(Signal<U, V> sig) :
            Signal<T, void>(cast<T>(std::move(sig)))
        {
        }

        Convert(Convert&&) = default;
        Convert& operator=(Convert&&) = default;

    private:
        Convert(Convert const&) = default;
        Convert& operator=(Convert const&) = default;
    };

} // reactive::signal

