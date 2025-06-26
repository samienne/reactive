#pragma once

#include <type_traits>
#include <utility>

namespace btl
{
    template <typename T>
    class ForceNoexcept
    {
    private:
        T t_;

    public:
        ForceNoexcept(T&& t) noexcept :
            t_(std::move(t))
        {
        }

        ForceNoexcept(T const& t) :
            t_(t)
        {
        }

        ForceNoexcept(T& t) :
            t_(t)
        {
        }

        ForceNoexcept(ForceNoexcept const& rhs) :
            t_(rhs.t_)
        {
        }

        ForceNoexcept(ForceNoexcept&& rhs) noexcept :
            t_(std::move(rhs.t_))
        {
        }

        ForceNoexcept& operator=(ForceNoexcept const& rhs)
        {
            if (this != &rhs)
                t_ = rhs.t_;

            return *this;
        }

        ForceNoexcept& operator=(ForceNoexcept&& rhs) noexcept
        {
            new (&t_) T(std::move(rhs.t_));
            return *this;
        }

        T& operator*() & noexcept
        {
            return t_;
        }

        T&& operator*() && noexcept
        {
            return t_;
        }

        T const& operator*() const& noexcept
        {
            return t_;
        }

        T* operator->() noexcept
        {
            return t_;
        }

        T const* operator->() const noexcept
        {
            return t_;
        }

        template <typename U>
        auto operator[](U&& u) -> decltype(t_[std::forward<U>(u)])
        {
            return t_[std::forward<U>(u)];
        }

        template <typename U>
        auto operator[](U&& u) const -> decltype(t_[std::forward<U>(u)])
        {
            return t_[std::forward<U>(u)];
        }

        template <typename U>
        auto operator==(U&& u) const -> decltype(t_ == std::forward<U>(u))
        {
            return t_ == std::forward<U>(u);
        }

        template <typename U>
        auto operator!=(U&& u) const -> decltype(t_ != std::forward<U>(u))
        {
            return t_ != std::forward<U>(u);
        }

        template <typename U>
        auto operator<(U&& u) const -> decltype(t_ < std::forward<U>(u))
        {
            return t_ < std::forward<U>(u);
        }

        template <typename U>
        auto operator>(U&& u) const -> decltype(t_ > std::forward<U>(u))
        {
            return t_ > std::forward<U>(u);
        }

        template <typename U>
        auto operator==(ForceNoexcept<U> const& rhs) const -> decltype(t_ == rhs.t_)
        {
            return t_ == rhs.t_;
        }

        template <typename U>
        auto operator!=(ForceNoexcept<U> const& rhs) const -> decltype(t_ != rhs.t_)
        {
            return t_ != rhs.t_;
        }

        template <typename U>
        auto operator<(ForceNoexcept<U> const& rhs) const -> decltype(t_ < rhs.t_)
        {
            return t_ < rhs.t_;
        }

        template <typename U>
        auto operator>(ForceNoexcept<U> const& rhs) const -> decltype(t_ > rhs.t_)
        {
            return t_ > rhs.t_;
        }
    };

    template <typename T>
    auto forceNoexcept(T&& t)
    {
        return ForceNoexcept<std::decay_t<T>>(std::forward<T>(t));
    }
} // btl

