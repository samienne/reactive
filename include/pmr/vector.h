#pragma once

#include "polymorphic_allocator.h"

#include <vector>

namespace pmr
{
    template <typename T>
    using vector = std::vector<T, polymorphic_allocator<T>>;
} // namespace pmr

