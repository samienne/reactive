#pragma once

#include "deleter.h"
#include "memory_resource.h"

#include <memory>

namespace pmr
{
    template <typename T>
    using unique_ptr = std::unique_ptr<T, deleter<T>>;

    template <typename T, typename... Ts>
    auto make_unique(pmr::memory_resource* memory, Ts&&... ts)
    {
        return unique_ptr<T>(
                new T(std::forward<Ts>(ts)...),
                deleter<T>(memory)
                );
    }
} // namespace pmr

