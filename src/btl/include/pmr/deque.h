#pragma once

#include "polymorphic_allocator.h"

#include <deque>

namespace pmr
{
    template <typename T>
    using deque = std::deque<T, polymorphic_allocator<T>>;
} // namespace pmr

