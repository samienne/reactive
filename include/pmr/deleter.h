#pragma once

#include "polymorphic_allocator.h"
#include "memory_resource.h"

namespace pmr
{
    template <typename T>
    class deleter
    {
    public:
        deleter(pmr::memory_resource* memory) :
            memory_(memory)
        {
        }

        void operator()(T* obj) noexcept
        {
            polymorphic_allocator<T> alloc(memory_);

            alloc.delete_object(obj);
        }

    private:
        pmr::memory_resource* memory_;
    };
} // namespace pmr

