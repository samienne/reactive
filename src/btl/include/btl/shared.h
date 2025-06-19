#pragma once

#include "typetraits.h"

#include <memory>
#include <type_traits>
#include <cassert>

namespace btl
{
    template <typename T>
    class shared
    {
    public:
        template <typename T2, typename T3 = T, typename = typename
            std::enable_if
            <
                std::is_base_of<T3, T2>::value
            >::type>
        shared(std::shared_ptr<T2> const& rhs) :
            data_(rhs)
        {
            assert(data_ && "empty pointer not allowed");
        }

        template <typename T2, typename T3 = T, typename = typename
            std::enable_if
            <
                std::is_base_of<T3, T2>::value
            >::type>
        shared(std::shared_ptr<T2>&& rhs) :
            data_(std::move(rhs))
        {
            assert(data_ && "empty pointer not allowed");
        }

        shared(shared<T> const&) = default;
        shared(shared<T>&&) noexcept = default;

        ~shared()
        {
        }

        shared& operator=(shared<T> const&) = default;
        shared& operator=(shared<T>&&) noexcept = default;

        T& operator*()
        {
            assert(data_ && "Use after move");
            return *data_;
        }

        T const& operator*() const
        {
            assert(data_ && "Use after move");
            return *data_;
        }

        T* operator->()
        {
            assert(data_ && "Use after move");
            return data_.get();
        }

        T const* operator->() const
        {
            assert(data_ && "Use after move");
            return data_.get();
        }

        std::shared_ptr<T> const& ptr() const
        {
            assert(data_ && "Use after move");
            return data_;
        }

        T const* raw_ptr() const
        {
            assert(data_ && "Use after move");
            return data_.get();
        }

        T* get()
        {
            assert(data_ && "Use after move");
            return data_.get();
        }

        T const* get() const
        {
            assert(data_ && "Use after move");
            return data_.get();
        }

        bool operator==(shared const& rhs) const
        {
            return data_ == rhs.data_;
        }

        bool operator!=(shared const& rhs) const
        {
            return data_ != rhs.data_;
        }

        bool operator<(shared const& rhs) const
        {
            return data_ < rhs.data_;
        }

        bool operator>(shared const& rhs) const
        {
            return data_ > rhs.data_;
        }
    private:
        std::shared_ptr<T> data_;
    };
}

