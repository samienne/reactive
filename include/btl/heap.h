#pragma once

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
    Heap(T&& t) :
        p_(std::make_unique<T>(std::move(t)))
    {
    }

     Heap(T const& t) :
        p_(std::make_unique<T>(t))
    {
    }

    template <typename U = T, typename =
        std::enable_if_t<std::is_default_constructible<U>::value>
    >
    Heap() :
        p_(std::make_unique<T>())
    {
    }

    Heap(Heap const& rhs) :
        p_(std::make_unique<T>(*rhs.p_))
    {
    }

    Heap(Heap&& rhs) noexcept = default;

    Heap& operator=(Heap const& rhs)
    {
        p_ = std::make_unique<T>(rhs.p_);
        return *this;
    }

    Heap& operator=(Heap&& rhs) noexcept = default;

    T& operator*() &
    {
        return *p_;
    }

    T const & operator*() const &
    {
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
    std::unique_ptr<T> p_;
};

template <typename T>
Heap<std::decay_t<T>> make_heap(T&& t)
{
    return Heap<std::decay_t<T>>(std::forward<T>(t));
}
} // namespace btl

