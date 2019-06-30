#pragma once

#include "memory_resource.h"
#include <memory>

namespace pmr
{
    class new_delete_resource_impl : public memory_resource
    {
    public:
        inline new_delete_resource_impl()
        {
        }

        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            std::size_t new_size = aligned_size(bytes, alignment);
            return alloc.allocate(new_size);
        }

        void do_deallocate(void* p, std::size_t bytes,
                std::size_t alignment) override
        {
            std::size_t new_size = aligned_size(bytes, alignment);
            return alloc.deallocate(reinterpret_cast<char*>(p), new_size);
        }

        bool do_is_equal(memory_resource const& other) const override
        {
            return this == &other;
        }

    private:
        inline std::size_t aligned_size(
                std::size_t size,
                std::size_t alignment)
        {
            return is_pow2(alignment)
                ? aligned_size_pow2(size, alignment)
                : alignof(std::max_align_t)
                ;
        }

        // Alignment must be pow of 2.
        static inline std::size_t aligned_size_pow2(
                std::size_t size, std::size_t alignment)
        {
            return ((size - 1) | (alignment -1)) + 1;
        }

        static inline bool is_pow2(std::size_t alignment)
        {
            return (alignment != 0) && !(alignment & (alignment-1));
        }

        std::allocator<char> alloc;
    };

    inline memory_resource* new_delete_resource() noexcept
    {
        static new_delete_resource_impl res;
        return &res;
    }
} // namespace pmr

