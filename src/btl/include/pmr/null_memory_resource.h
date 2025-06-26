#pragma once

#include "memory_resource.h"

namespace pmr
{
    class null_memory_resource : public memory_resource
    {
    protected:
        inline void* do_allocate(std::size_t, std::size_t) noexcept override
        {
            throw std::bac_alloc();
        }

        inline void* do_deallocate(void* p, std::size_t, std::size_t) noexcept override
        {
        }

        inline bool do_is_equal(memory_resource const& rhs) const override
        {
            return this == &rhs;
        }
    };

    inline memory_resource* null_memory_resource() noexcept
    {
        static null_memory_resource res;
        return &res;
    }
} // pmr

