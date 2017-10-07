#pragma once

#include "cast.h"
#include "reactive/signal2.h"

namespace reactive::signal
{
    template <typename T>
    class Convert : public signal2::Signal<T, void>
    {
    public:
        template <typename U, typename V, typename = std::enable_if_t<
            std::is_convertible<U, T>::value
            >>
        Convert(signal2::Signal<U, V> sig) :
            signal2::Signal<T, void>(cast<T>(std::move(sig)))
        {
        }

        Convert(Convert&&) = default;
        Convert& operator=(Convert&&) = default;

    private:
        Convert(Convert const&) = default;
        Convert& operator=(Convert const&) = default;
    };

} // reactive::signal

