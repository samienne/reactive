#pragma once

#include "option.h"
#include "fmap.h"
#include "voidt.h"
#include "not.h"
#include "all.h"

#include <vector>
#include <utility>
#include <type_traits>

namespace btl
{
    template <typename T, typename = void>
    struct Clone
    {
        template <typename U = T, typename = std::enable_if_t<
            std::is_copy_constructible<U>::value
        >>
        static U clone(U const& u)
        {
            return U(u);
        }
    };

    template <typename T>
    auto clone(T const& t) -> decltype(Clone<T>::clone(t))
    {
        return Clone<T>::clone(t);
    }

    template <typename T>
    auto clone(T&& t) -> std::enable_if_t<
    btl::All<std::is_rvalue_reference<T&&>, btl::Not<std::is_const<T>>>::value,
        std::decay_t<T>>
    {
        return std::move(t);
    }

    template <typename T>
    using clone_t = decltype(btl::clone(std::declval<std::decay_t<T> const>()));

    template <typename T, typename = void>
    struct IsClonable : std::false_type {};

    template <typename T>
    struct IsClonable<T, void_t<clone_t<T>>> :
        btl::Not<std::is_pointer<clone_t<T>>> {};

    template <typename T>
    struct Clone<T, btl::void_t<
        decltype(std::declval<T const>().clone())
        >
    >
    {
        static auto clone(T const& t)
            -> decltype(t.clone())
        {
            return t.clone();
        }
    };

    template <typename T>
    struct Clone<std::vector<T>, std::enable_if_t<
        IsClonable<T>::value
    >>
    {
        static std::vector<T> clone(std::vector<T> const& v)
        {
            return btl::fmap(v, [](T const& t)
                    -> decltype(btl::clone(t))
            {
                return btl::clone(t);
            });
        }
    };

    template <typename... Ts>
    struct Clone<std::tuple<Ts...>, std::enable_if_t<
        btl::All<
            IsClonable<Ts>...
        >::value
    >>
    {
        static std::tuple<Ts...> clone(std::tuple<Ts...> const& t)
        {
            return btl::tuple_map(t, [](auto const& v)
            {
                return btl::clone(v);
            });
        }
    };

    template <typename T>
    struct Clone<btl::option<T>, std::enable_if_t<
        IsClonable<T>::value
    >>
    {
        static btl::option<T> clone(btl::option<T> const& v)
        {
            if (!v.valid())
                return btl::none;

            return btl::just(btl::clone(*v));
        }
    };

    template <typename T>
    struct Clone<T&&, std::enable_if_t<
        std::is_rvalue_reference<T&&>::value
        >
    >
    {
        static T&& clone(T&& v)
        {
            return std::move(v);
        }
    };

    template <typename T>
    class CloneOnCopy
    {
    public:
        static_assert(IsClonable<T>::value, "T is not clonable");

        CloneOnCopy(T t) :
            value_(std::move(t))
        {
        }

        CloneOnCopy(CloneOnCopy const& rhs) :
            value_(btl::clone(rhs.value_))
        {
        }

        CloneOnCopy(CloneOnCopy&& rhs) noexcept :
            value_(std::move(rhs.value_))
        {
        }

        CloneOnCopy& operator=(CloneOnCopy const& rhs)
        {
            if (&rhs == this)
                return *this;

            value_ = btl::clone(rhs);

            return *this;
        }

        CloneOnCopy& operator=(CloneOnCopy&& rhs) noexcept
        {
            if (&rhs == this)
                return *this;

            value_ = std::move(*rhs);

            return *this;
        }

        T* operator->()
        {
            return &value_;
        }

        T const* operator->() const
        {
            return &value_;
        }

        T& operator*() &
        {
            return value_;
        }

        T operator*() &&
        {
            return std::move(value_);
        }

        T const& operator*() const&
        {
            return value_;
        }

    private:
        T value_;
    };

    template <typename T, typename = std::enable_if_t<
        All<
            IsClonable<std::decay_t<T>>,
            std::is_move_constructible<std::decay_t<T>>
        >::value
        >
    >
    auto cloneOnCopy(T t) -> CloneOnCopy<std::decay_t<T>>
    {
        return CloneOnCopy<std::decay_t<T>>(std::move(t));
    }
} // btl

