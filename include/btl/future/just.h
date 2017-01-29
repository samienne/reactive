#pragma once

#include "future.h"
#include <btl/option.h>

namespace btl
{
    namespace future
    {
        template <typename T>
        auto just(Future<T> f) -> Future<btl::option<T>>
        {
            return std::move(f).fmap([](auto&& v)
            {
                return btl::just(v);
            });
        }
    } // future
} // btl

