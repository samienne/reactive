#pragma once

#include "polymorphic_allocator.h"

#include <vector>

namespace pmr
{
    template <typename T>
    using vector = std::vector<T, polymorphic_allocator<T>>;

    template <typename T>
    auto with_resource(pmr::memory_resource* memory, pmr::vector<T>&& vec)
    {
        if (memory == vec.get_allocator().resource())
            return std::move(vec);

        pmr::vector<T> result(memory);
        result.reserve(vec.size());

        for (auto&& v : vec)
            result.push_back(with_resource(std::move(v)));

        return result;
    }
} // namespace pmr

