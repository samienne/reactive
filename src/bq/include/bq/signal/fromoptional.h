#pragma once

#include "signal.h"

#include <optional>

namespace bq::signal
{
    template <typename T, typename U>
    AnySignal<std::optional<T>> fromOptional(std::optional<Signal<U, T>> sig)
    {
        if (sig)
            return sig->makeOptional();
        else
        {
            return constant<std::optional<T>>(std::nullopt);
        }
    }

    template <typename T>
    AnySignal<std::optional<T>> fromOptional(std::optional<AnySignal<T>> sig)
    {
        if (sig)
            return sig->makeOptional();
        else
        {
            return constant<std::optional<T>>(std::nullopt);
        }
    }
} // namespace bq::signal

