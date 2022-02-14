#pragma once

#include "future.h"

#include <optional>

namespace btl::future
{
    template <typename T>
    auto just(Future<T> f) -> Future<std::optional<T>>
    {
        return std::move(f).fmap([](auto&& v)
        {
            return std::make_optional(v);
        });
    }
} // namespace btl

