#pragma once

#include "signaltraits.h"

#include <btl/all.h>

namespace reactive::signal2
{
    template <typename TStorage, typename... Ts>
    class Signal;

    template <typename TStorage, typename TResult>
    struct GetSignalTypeFromResult
    {
        using type = Signal<TStorage, std::decay_t<TResult>>;
    };

    template <typename TStorage, typename... Ts>
    struct GetSignalTypeFromResult<TStorage, SignalResult<Ts...>>
    {
        using type = Signal<TStorage, std::decay_t<Ts>...>;
    };

    template <typename TStorage, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<TStorage, std::decay_t<TStorage>>,
            IsSignal<TStorage>
        >::value
        >>
    typename GetSignalTypeFromResult<TStorage, std::decay_t<SignalTypeT<TStorage>>>::type
    wrap(TStorage&& sig)
    {
        return { std::forward<TStorage>(sig) };
    }
} // namespace reactive::signal2

