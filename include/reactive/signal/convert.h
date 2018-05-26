#pragma once

#include "cast.h"
#include "reactive/signal.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename T>
        class BTL_CLASS_VISIBLE Convert;
    }

    template <typename T>
    struct IsSignal<signal::Convert<T>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename T>
    class BTL_CLASS_VISIBLE Convert : public Signal<T, void>
    {
    public:
        template <typename U, typename V, typename = std::enable_if_t<
            std::is_convertible<U, T>::value
            >>
        BTL_HIDDEN Convert(Signal<U, V> sig) :
            Signal<T, void>(cast<T>(std::move(sig)))
        {
        }

        BTL_HIDDEN Convert(Convert&&) = default;
        BTL_HIDDEN Convert& operator=(Convert&&) = default;

    private:
        BTL_HIDDEN Convert(Convert const&) = default;
        BTL_HIDDEN Convert& operator=(Convert const&) = default;
    };

} // reactive::signal

BTL_VISIBILITY_POP

