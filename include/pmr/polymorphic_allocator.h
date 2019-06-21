#pragma once

#include "memory_resource.h"

#include <utility>

namespace pmr
{
    template <typename T = std::byte>
    class polymorphic_allocator
    {
    public:
        using value_type = T;

        polymorphic_allocator(memory_resource* resource) :
            resource_(resource)
        {
        }

        polymorphic_allocator(polymorphic_allocator const& rhs) noexcept = default;

        template <typename U>
        polymorphic_allocator(polymorphic_allocator<U> const& rhs) :
            polymorphic_allocator(rhs.resource())
        {
        }

        ~polymorphic_allocator() = default;

        [[nodiscard]]
        T* allocate(std::size_t n)
        {
            return allocate_object<T>(n);
        }

        void deallocate(T* p, std::size_t n)
        {
            deallocate_object<T>(p, n);
        }

        template <typename U, typename... TArgs>
        void construct(U* p, TArgs&&... args)
        {
            new (p)U(std::forward<TArgs>(args)...);
        }

        template <typename U>
        void destroy(U* u)
        {
            u->~U();
        }

        void* allocate_bytes(
                std::size_t nbytes,
                std::size_t alignment = alignof(std::max_align_t)
                )
        {
            return resource_->allocate(nbytes, alignment);
        }

        void deallocate_bytes(
                void* p,
                std::size_t nbytes,
                std::size_t alignment = alignof(std::max_align_t)
                )
        {
            resource_->deallocate(p, nbytes, alignment);
        }

        template <typename U>
        U* allocate_object(std::size_t n = 1)
        {
            return reinterpret_cast<U*>(
                    allocate_bytes(sizeof(U) * n, alignof(U))
                    );
        }

        template <typename U>
        void deallocate_object(U* p, std::size_t n = 1)
        {
            deallocate_bytes(p, sizeof(U) * n, alignof(U));
        }

        template <typename U, typename... TArgs>
        U* new_object(TArgs&&... args)
        {
            U* p = allocate_object<U>();

            try
            {
                construct<U>(p, std::forward<TArgs>(args)...);
            }
            catch(...)
            {
                deallocate_object<U>(p);
                throw;
            }

            return p;
        }

        template <typename U>
        void delete_object(U* p)
        {
            destroy<U>(p);
            deallocate_object<U>(p);
        }

        polymorphic_allocator select_on_container_copy_construction() const
        {
            return *this;
        }

        inline memory_resource* resource() const
        {
            return resource_;
        }

    private:
        memory_resource* resource_;
    };

    template <typename T, typename U>
    bool operator==(
            polymorphic_allocator<T> const& lhs,
            polymorphic_allocator<T> const& rhs
            )
    {
        return lhs.resource() == rhs.resource();
    }

    template <typename T, typename U>
    bool operator!=(
            polymorphic_allocator<T> const& lhs,
            polymorphic_allocator<T> const& rhs
            )
    {
        return !(lhs == rhs);
    }
} // namespace pmr

