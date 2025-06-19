#pragma once

#include "polymorphic_allocator.h"

#include <memory>

namespace pmr
{
    template <typename T, typename... Ts>
    std::shared_ptr<T> make_shared(memory_resource* memory, Ts&&... ts)
    {
        return std::allocate_shared<T>(
                polymorphic_allocator<T>(memory),
                std::forward<Ts>(ts)...
                );
    }
} // namespace pmr

