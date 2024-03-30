#pragma once

#include "constant.h"

#include <optional>

namespace reactive::signal
{
    template <typename T>
    AnySignal<std::optional<T>> flipOptional(std::optional<AnySignal<T>> s)
    {
        if (!s)
            return signal::constant<std::optional<T>>(std::nullopt);

        return std::move(*s).map([](auto value)
            {
                return std::make_optional(std::move(value));
            });
    }
} // namespace reactive::signal
