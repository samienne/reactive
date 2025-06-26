#pragma once

#include "deque.h"
#include "polymorphic_allocator.h"

#include <queue>

namespace pmr
{
    template <typename T>
    using queue = std::queue<T, pmr::deque<T>>;
} // namespace pmr

