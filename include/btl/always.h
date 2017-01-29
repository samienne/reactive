#pragma once

#include <type_traits>
#include <utility>

namespace btl
{
    template <typename T>
    class Always
    {
    public:
        Always(T&& t) :
            value_(std::move(t))
        {
        }

        Always(T const& t) :
            value_(t)
        {
        }

        T const& operator()() const & noexcept
        {
            return value_;
        }

        T& operator()() & noexcept
        {
            return value_;
        }

        T operator()() && noexcept
        {
            return std::move(value_);
        }

    private:
        T value_;
    };

    template <typename T>
    Always<std::decay_t<T>> always(T&& t)
    {
        return Always<std::decay_t<T>>(std::forward<T>(t));
    }
} // btl

