#pragma once

#include "unique_ptr.h"
#include "memory_resource.h"

namespace pmr
{

    /**
    * @brief Stores an object in heap.
    *
    * Creates automatic copy of the object in copy constructor and copy
    * assignment.
    */
    template<typename T>
    class heap
    {
    public:
        heap(T&& t, memory_resource* memory) :
            p_(make_unique<T>(memory, std::move(t)))
        {
        }

        heap(T const& t, memory_resource* memory) :
            p_(make_unique<T>(memory, t))
        {
        }

        heap(heap const& rhs) :
            p_(make_unique<T>(rhs.resource(), *rhs.p_))
        {
            assert(p_.get());
        }

        heap(heap&& rhs) noexcept = default;

        heap& operator=(heap const& rhs)
        {
            p_ = make_unique<T>(rhs.resource(), *rhs.p_);
            assert(p_.get());
            return *this;
        }

        heap& operator=(heap&& rhs) noexcept = default;

        memory_resource* resource() const
        {
            return p_.get_deleter().resource();
        }

        T& operator*() &
        {
            assert(p_.get());
            return *p_;
        }

        T const & operator*() const &
        {
            assert(p_.get());
            return *p_;
        }

        T& operator*() &&
        {
            return std::move(*p_);
        }

        T* operator->()
        {
            return p_.get();
        }

        T const* operator->() const
        {
            return p_.get();
        }

        bool operator==(heap const& rhs) const
        {
            return p_ == rhs.p_;
        }

        bool operator!=(heap const& rhs) const
        {
            return p_ != rhs.p_;
        }

        bool operator<(heap const& rhs) const
        {
            return p_ < rhs.p_;
        }

        bool operator>(heap const& rhs) const
        {
            return p_ < rhs.p_;
        }

    private:
        unique_ptr<T> p_;
    };

    template <typename T, typename... Ts>
    heap<T> make_heap(memory_resource* memory, Ts&&... ts)
    {
        return heap<T>(T{ std::forward<Ts>(ts)... }, memory);
    }
} // namespace pmr

