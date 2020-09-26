#pragma once

#include "typetraits.h"
#include "hash.h"

#include <type_traits>
#include <utility>
#include <cassert>
#include <ostream>

namespace btl
{
    struct none_t
    {
    };

    static constexpr none_t none{};

    template <typename T> class option;

    template <typename T>
    auto just(T&& value) -> btl::option<std::decay_t<T>>;

    template <typename T>
    class alignas(alignof(T)) option
    {
    public:
        option(none_t = none) noexcept
        {
        }

        option(option<T> const& rhs) :
            valid_(rhs.valid_)
        {
            if (rhs.valid_)
                new (&value())T(rhs.value());
        }

        option(option<T>&& rhs) noexcept:
            valid_(rhs.valid_)
        {
            if (rhs.valid_)
                new (&value())T(std::move(rhs.value()));
        }

    private:
        template <typename T2>
        friend btl::option<typename std::decay<T2>::type> just(T2&& value);

        template <typename U>
        friend class btl::option;

        option(T const& v) :
            valid_(true)
        {
            new (&value())T(v);
        }

        option(T&& v) noexcept:
            valid_(true)
        {
            new (&value())T(std::forward<T>(v));
        }

    public:

        ~option()
        {
            if (valid_)
                reinterpret_cast<T*>(data_.data())->~T();
        }

        option& operator=(option const& rhs)
        {
            if (this == &rhs)
                return *this;

            if (valid_)
            {
                reinterpret_cast<T*>(data_.data())->~T();
                valid_ = false;
            }

            if (rhs.valid_)
            {
                new (&value())T(rhs.value());
            }

            valid_ = rhs.valid_;
            return *this;
        }

        option& operator=(option&& rhs) noexcept
        {
            if (this == &rhs)
                return *this;

            if (valid_)
            {
                reinterpret_cast<T*>(data_.data())->~T();
                valid_ = false;
            }

            if (rhs.valid_)
            {
                new (&value())T(std::move(rhs.value()));
            }

            valid_ = rhs.valid_;
            return *this;
        }

        option& operator=(none_t const&) noexcept
        {
            if (valid_)
                reinterpret_cast<T*>(data_.data())->~T();
            valid_ = false;

            return *this;
        }

        T& operator*() &
        {
            assert(valid_);
            return *reinterpret_cast<T*>(data_.data());
        }

        T const& operator*() const &
        {
            assert(valid_);
            return *reinterpret_cast<T const*>(data_.data());
        }

        T&& operator*() &&
        {
            assert(valid_);
            return std::move(*reinterpret_cast<T*>(data_.data()));
        }

        T* operator->() &
        {
            assert(valid_);
            return reinterpret_cast<T*>(data_.data());
        }

        T const* operator->() const &
        {
            assert(valid_);
            return reinterpret_cast<T const*>(data_.data());
        }

        bool valid() const
        {
            return valid_;
        }

        bool empty() const
        {
            return !valid_;
        }

#ifdef __SANITIZE_THREAD__
        T const& getReferenceForTsan() const
        {
            return *reinterpret_cast<T const*>(data_.data());
        }
#endif

        bool operator==(option<T> const& rhs) const noexcept
        {
            if (valid_ != rhs.valid_)
                return false;

            if (valid_)
            {
                return *reinterpret_cast<T const*>(data_.data()) ==
                    *reinterpret_cast<T const*>(rhs.data_.data());
            }

            return true;
        }

        bool operator!=(option<T> const& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        bool operator<(option<T> const& rhs) const noexcept
        {
            if (valid_ != rhs.valid_)
                return !valid_;

            if (valid_)
                return value() < rhs.value();

            return false;
        }

        bool operator>(option<T> const& rhs) const noexcept
        {
            if (valid_ != rhs.valid_)
                return valid_;

            if (valid_)
                return value() > rhs.value();

            return false;
        }

        template <typename TLhs, typename = typename btl::enable_if_t<
            !std::is_same<btl::decay_t<TLhs>, option<T>>::value
            >>
        friend auto operator*(TLhs&& lhs, option<T> const& rhs)
        -> option<decltype(lhs * *rhs)>
        {
            if (rhs.empty())
                return option<T>(none);

            return just(std::forward<TLhs>(lhs) * *rhs);
        }

        template <typename TRhs, typename = typename btl::enable_if_t<
            !std::is_same<btl::decay_t<TRhs>, option<T>>::value
            >>
        friend auto operator*(option<T> const& lhs, TRhs&& rhs)
        -> option<decltype(*lhs * rhs)>
        {
            if (lhs.empty())
                return option<T>(none);

            return just(*lhs * std::forward<TRhs>(rhs));
        }

        template <typename TRhs>
        friend auto operator*(option<T> const& lhs, option<TRhs> const& rhs)
        -> option<decltype(*lhs * *rhs)>
        {
            if (lhs.empty() || rhs.empty())
                return option<T>(none);

            return *lhs * *rhs;
        }

        template <typename TRhs, typename = void_t<multiply_t<T, TRhs>>>
        option<T>& operator*=(TRhs&& rhs)
        {
            if (valid_)
                value() *= std::forward<TRhs>(rhs);
            return *this;
        }

        template <class THash>
        friend void hash_append(THash& h, option<T> const& o) noexcept
        {
            using btl::hash_append;
            if (o.valid_)
                hash_append(h, o.value());
        }

        friend std::ostream& operator<<(std::ostream& stream, option const& o)
        {
            if (o.valid())
                return stream << "O(" << *o << ")";
            else
                return stream << "O(none)";
        }

        template <typename U, typename = std::enable_if_t<
            std::is_convertible<T, U>::value
            >
        >
        operator btl::option<U>() &&
        {
            if (!valid_)
                return btl::none;

            valid_ = false;

            return btl::just<U>(std::move(value()));
        }

        template <typename U, typename = std::enable_if_t<
            std::is_convertible<T const, U>::value
            >
        >
        operator btl::option<U>() const &
        {
            if (!valid_)
                return btl::none;

            return btl::just<U>(value());
        }

    private:
        T& value() { return *reinterpret_cast<T*>(data_.data()); }
        T const& value() const {
            return *reinterpret_cast<T const*>(data_.data()); }

    private:
        std::array<char, sizeof(T)> data_;
        bool valid_ = false;
    };

    namespace detail
    {
        template <typename T>
        struct option_type
        {
            using type = T;
        };

        template <typename T>
        struct option_type<option<T>> : option_type<T> {};

        template <typename T>
        using option_type_t = typename option_type<T>::type;

        template <typename T>
        struct option_val
        {
            auto operator()(T&& v)
                -> decltype(std::forward<T>(std::declval<T>()))
            {
                return std::forward<T>(v);
            }
        };

        template <typename T>
        struct option_val<option<T>>
        {
            auto operator()(option<T> const& v) -> T
            {
                return *v;
            }
        };

        template <typename T>
        struct is_none
        {
            bool operator()(T const&) const
            {
                return false;
            }
        };

        template <typename T>
        struct is_none<option<T>>
        {
            bool operator()(option<T> const& v) const
            {
                return v.none();
            }
        };

        template <typename T, typename... Ts>
        struct any
        {
            auto operator()(T const& v, Ts const&... vs) -> bool
            {
                return v || any<Ts...>()(vs...);
            }
        };

        template <typename T>
        struct any<T>
        {
            auto operator()(T const& v) -> bool
            {
                return v;
            }
        };
    }

    template <typename T>
    bool operator==(btl::none_t const&, btl::option<T> const& o)
    {
        return !o.valid();
    }

    template <typename T>
    bool operator!=(btl::none_t const&, btl::option<T> const& o)
    {
        return o.valid();
    }

    template <typename TFunc, typename... Ts>
    auto mapOption(TFunc&& f, Ts&&... vs) -> option<typename std::decay
    <
        decltype(std::declval<TFunc>()(
                    std::declval<detail::option_type_t<Ts>::type>()...
                    ))
    >::type>
    {
        if (detail::any<Ts...>()(detail::is_none<Ts>(vs)...))
            return none;

        return f(detail::option_val<Ts>()(vs)...);
    }

    template <typename T>
    auto just(T&& value) -> btl::option<std::decay_t<T>>
    {
        return btl::option<std::decay_t<T>>(std::forward<T>(value));
    }

    inline auto just(char const* str) -> btl::option<std::string>
    {
        return just<std::string>(str);
    }

    template <int n>
    inline auto just(char const str[n]) -> btl::option<std::string>
    {
        return just<std::string>(std::string(str));
    }
}

