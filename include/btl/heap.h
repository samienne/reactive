#pragma once

#include <pmr/unique_ptr.h>
#include <pmr/memory_resource.h>

#include <memory>
#include <type_traits>

namespace btl
{

/**
 * @brief Stores an object in heap.
 *
 * Creates automatic copy of the object in copy constructor and copy
 * assignment.
 */
template<typename T>
class Heap
{
public:
    Heap(T&& t, pmr::memory_resource* memory) :
        p_(pmr::make_unique<T>(memory, std::move(t)))
    {
    }

     Heap(T const& t, pmr::memory_resource* memory) :
        p_(pmr::make_unique<T>(memory, t))
    {
    }

    /*
    template <typename U = T, typename =
        std::enable_if_t<std::is_default_constructible<U>::value>
    >
    Heap(pmr::memory_resource* memory) :
        p_(pmr::make_unique<T>(memory))
    {
    }
    */

    Heap(Heap const& rhs) :
        p_(pmr::make_unique<T>(rhs.getResource(), *rhs.p_))
    {
        assert(p_.get());
    }

    Heap(Heap&& rhs) noexcept = default;

    Heap& operator=(Heap const& rhs)
    {
        p_ = pmr::make_unique<T>(rhs.getResource(), rhs.p_);
        assert(p_.get());
        return *this;
    }

    Heap& operator=(Heap&& rhs) noexcept = default;

    pmr::memory_resource* getResource() const
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

    bool operator==(Heap const& rhs) const
    {
        return p_ == rhs.p_;
    }

    bool operator!=(Heap const& rhs) const
    {
        return p_ != rhs.p_;
    }

    bool operator<(Heap const& rhs) const
    {
        return p_ < rhs.p_;
    }

    bool operator>(Heap const& rhs) const
    {
        return p_ < rhs.p_;
    }

private:
    pmr::unique_ptr<T> p_;
};

template <typename T, typename... Ts>
Heap<T> make_heap(pmr::memory_resource* memory, Ts&&... ts)
{
    return Heap<T>(T{ std::forward<Ts>(ts)... }, memory);
}
} // namespace btl

