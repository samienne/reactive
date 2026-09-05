#pragma once

#include "tupleallbutlast.h"

#include <tuple>
#include <utility>

namespace btl
{
    /**
     * @brief Adapts @p func to accept and ignore one extra trailing argument.
     *
     * The wrapper forwards all but its last argument to @p func, letting a
     * callable that ignores a trailing event stand in where one is supplied.
     */
    template <typename TFunc>
    auto ignoreLast(TFunc&& func)
    {
        return [func = std::forward<TFunc>(func)](auto&&... us) mutable
        {
            return std::apply(
                    func,
                    tuple_all_but_last(
                        std::forward_as_tuple(
                            std::forward<decltype(us)>(us)...)));
        };
    }
} // namespace btl
