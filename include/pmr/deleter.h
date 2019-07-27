#pragma once

#include "polymorphic_allocator.h"
#include "memory_resource.h"

namespace pmr
{
    template <typename T>
    class deleter
    {
    public:
        deleter(memory_resource* memory) :
            memory_(memory)
        {
        }

        void operator()(T* obj) noexcept
        {
            polymorphic_allocator<T> alloc(memory_);

            alloc.delete_object(obj);
        }

        memory_resource* resource() const
        {
            return memory_;
        }

    private:
        memory_resource* memory_;
    };
} // namespace pmr

