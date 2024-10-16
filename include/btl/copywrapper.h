#pragma once

#include <utility>

namespace btl
{
    template <typename T>
    class CopyWrapper
    {
    public:
        CopyWrapper(T value) :
            value_(std::move(value))
        {
        }

        CopyWrapper(CopyWrapper const&) = default;

        CopyWrapper(CopyWrapper&& rhs) noexcept :
            value_(std::move(rhs.value_))
        {
        }

        template <typename U, typename = std::enable_if_t<
            std::is_constructible_v<T, U&&>
            >>
        CopyWrapper(U&& rhs) :
            value_(std::forward<U>(rhs))
        {
        }

        template <typename U, typename = std::enable_if_t<
            std::is_constructible_v<T, U const&>
            >>
        CopyWrapper(CopyWrapper<U> const& rhs) :
            value_(rhs.value)
        {
        }

        template <typename U, typename = std::enable_if_t<
            std::is_constructible_v<T, U&&>
            >>
        CopyWrapper(CopyWrapper<U>&& rhs) :
            value_(std::move(rhs.value))
        {
        }

        CopyWrapper& operator=(CopyWrapper const& rhs)
        {
            value_.~T();
            new (&value_) T(rhs.value_);
            return *this;
        }

        CopyWrapper& operator=(CopyWrapper&& rhs) noexcept
        {
            value_.~T();
            new (&value_) T(std::move(rhs.value_));
            return *this;
        }

        template <typename U, typename = std::enable_if_t<
            std::is_constructible_v<T, U&&>
            >>
        CopyWrapper& operator=(U&& rhs)
        {
            value_.~T();
            new (&value_) T(std::forward<U>(rhs));
            return *this;
        }

        T& operator*() &
        {
            return value_;
        }

        T const& operator*() const&
        {
            return value_;
        }

        T&& operator*() &&
        {
            return std::move(value_);
        }

        T* operator->()
        {
            return &value_;
        }

        T const* operator->() const
        {
            return &value_;
        }

    private:
        T value_;
    };

    template <typename T>
    CopyWrapper<std::decay_t<T>> copyWrap(T&& t)
    {
        return { std::forward<T>(t) };
    }
} // namespace btl

