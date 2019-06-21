#pragma once

#include <cstddef>

namespace pmr
{
    class memory_resource
    {
    public:
        virtual ~memory_resource() = default;

        [[nodiscard]]
        inline void* allocate(
                std::size_t bytes,
                std::size_t alignment = alignof(std::max_align_t)
                )
        {
            return do_allocate(bytes, alignment);
        }

        inline void deallocate(
                void* p,
                std::size_t bytes,
                std::size_t alignment = alignof(std::max_align_t)
                )
        {
            do_deallocate(p, bytes, alignment);
        }

        inline bool is_equal(memory_resource const& other) const
        {
            return do_is_equal(other);
        }

    private:
        virtual void* do_allocate(std::size_t bytes, std::size_t alignment) = 0;
        virtual void do_deallocate(void* p, std::size_t bytes,
                std::size_t alignment) = 0;
        virtual bool do_is_equal(memory_resource const& other) const = 0;
    };

    inline bool operator==(memory_resource const& a,
            memory_resource const& b) noexcept
    {
        return &a == &b || a.is_equal(b);
    }

    inline bool operator!=(memory_resource const& a,
            memory_resource const& b) noexcept
    {
        return !(a == b);
    }
} // namespace pmr

