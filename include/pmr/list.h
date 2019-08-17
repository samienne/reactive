#pragma once

#include "polymorphic_allocator.h"

#include <list>

namespace pmr
{
    template <typename T>
    using list = std::list<T, polymorphic_allocator<T>>;
} // namespace pmr


