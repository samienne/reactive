#pragma once

#include "typetraits.h"
#include "hash.h"

#include <algorithm>
#include <typeindex>
#include <cassert>

namespace btl
{
    template <typename... Ts> class variant;

    namespace detail
    {
        template <typename T, typename... Ts>
        struct MaxSizeOf
        {
            static const size_t value = sizeof(T) < MaxSizeOf<Ts...>::value ?
                MaxSizeOf<Ts...>::value : sizeof(T);

        };

        template <typename T>
        struct MaxSizeOf<T>
        {
            static const size_t value = sizeof(T);
        };

        template <typename T>
        void copyAs(void* target, void const* source)
        {
            T const& rhs = *reinterpret_cast<T const*>(source);

            new (target)T(rhs);
        }

        template <typename T, typename... Ts>
        struct TryCopyAs
        {
            bool operator()(std::type_index const& type, void* target,
                    void const* source)
            {
                if (type != typeid(T))
                    return TryCopyAs<Ts...>()(type, target, source);

                copyAs<T>(target, source);
                return true;
            }
        };

        template <typename T>
        struct TryCopyAs<T>
        {
            bool operator()(std::type_index const& type, void* target,
                    void const* source)
            {
                if (type != typeid(T))
                    return false;

                copyAs<T>(target, source);
                return true;
            }
        };

        template <typename T>
        void moveAs(void* target, void* source)
        {
            T& rhs = *reinterpret_cast<T*>(source);

            new (target)T(std::move(rhs));
        }

        template <typename T, typename... Ts>
        struct TryMoveAs
        {
            bool operator()(std::type_index const& type, void* target,
                    void* source)
            {
                if (type != typeid(T))
                    return TryCopyAs<Ts...>()(type, target, source);

                moveAs<T>(target, source);
                return true;
            }
        };

        template <typename T>
        struct TryMoveAs<T>
        {
            bool operator()(std::type_index const& type, void* target,
                    void* source)
            {
                if (type != typeid(T))
                    return false;

                moveAs<T>(target, source);
                return true;
            }
        };

        template <typename T>
        void deconstructAs(void* obj)
        {
            reinterpret_cast<T*>(obj)->~T();
        }

        template <typename T, typename... Ts>
        struct TryDeconstructAs
        {
            void operator()(std::type_index const& type, void* obj)
            {
                if (type != typeid(T))
                {
                    TryDeconstructAs<Ts...>()(type, obj);
                    return;
                }

                deconstructAs<T>(obj);
            }
        };

        template <typename T>
        struct TryDeconstructAs<T>
        {
            void operator()(std::type_index const& type, void* obj)
            {
                if (type != typeid(T))
                    return;

                deconstructAs<T>(obj);
            }
        };

        template <typename THash, typename T, typename... Ts>
        struct TryHashAs
        {
            bool operator()(THash& h, std::type_index const& type,
                    void const* target)
            {
                if (type != typeid(T))
                    return TryHashAs<THash, Ts...>()(h, type, target);

                hash_append(h, *static_cast<T const*>(target));
                return true;
            }
        };

        template <typename THash, typename T>
        struct TryHashAs<THash, T>
        {
            bool operator()(THash& h, std::type_index const& type,
                    void const* target)
            {
                if (type != typeid(T))
                    return false;

                hash_append(h, *static_cast<T const*>(target));
                return true;
            }
        };

        template <typename T, typename... Ts>
        struct AreNothrowMoveAssignable
        {
            static const bool value = std::is_nothrow_move_assignable<T>::value
                || AreNothrowMoveAssignable<Ts...>::value;
        };

        template <typename T>
        struct AreNothrowMoveAssignable<T> : std::is_nothrow_move_assignable<T>
        {
        };

        template <typename T, typename... Ts>
        struct AreNothrowMoveConstructible
        {
            static const bool value =
                std::is_nothrow_move_constructible<T>::value
                || AreNothrowMoveAssignable<Ts...>::value;
        };

        template <typename T>
        struct AreNothrowMoveConstructible<T> :
        std::is_nothrow_move_constructible<T>
        {
        };

        template <typename T, typename TVariant>
        struct HasType : std::false_type { };

        template <typename T, typename... Ts>
        struct HasType<T, variant<Ts...>> : IsOneOf<T, Ts...> { };

        template <typename TVariantA, typename TVariantB>
        struct IsAssignableTo : std::false_type {};

        template <typename TVariant, typename T, typename... Ts>
        struct IsAssignableTo<TVariant, variant<T, Ts...>>
        {
            static const bool value = HasType<T, TVariant>::value
                && IsAssignableTo<TVariant, variant<Ts...>>::value;
        };

        template <typename TVariant, typename T>
        struct IsAssignableTo<TVariant, variant<T>> : HasType<T, TVariant>{};

        template <typename T, typename... Ts>
        struct TryCompareEqual
        {
            bool operator()(std::type_index const& type,
                    void const* lhs, void const* rhs)
            {
                if (type != typeid(T))
                    return TryCompareEqual<Ts...>()(type, lhs, rhs);

                return *static_cast<T const*>(lhs) ==
                        *static_cast<T const*>(rhs);
            }
        };

        template <typename T>
        struct TryCompareEqual<T>
        {
            bool operator()(std::type_index const& type,
                    void const* lhs, void const* rhs)
            {
                if (type != typeid(T))
                    return false;

                return *static_cast<T const*>(lhs) ==
                        *static_cast<T const*>(rhs);
            }
        };

        template <typename T1, typename T2>
        using add_t = decltype(std::declval<T1>() + std::declval<T2>());

        template <typename TLhs, typename TRhs, typename = void>
        struct CanAdd : std::false_type {};

        template <typename TLhs, typename TRhs>
        struct CanAdd<TLhs, TRhs, VoidType<add_t<TLhs, TRhs>>> :
            std::true_type {};

        template <typename TRhs, typename TLhs, typename... TLhss>
        struct AllCanAdd
        {
            static const bool value = CanAdd<TLhs, TRhs>::value
                && AllCanAdd<TRhs, TLhss...>::value;
        };

        template <typename TRhs, typename TLhs>
        struct AllCanAdd<TRhs, TLhs>
        {
            static const bool value = CanAdd<TLhs, TRhs>::value;
        };

        template <typename TRet, typename TRhs, typename T, typename... Ts>
        struct TryAdd
        {
            TRet operator()(std::type_index const& type,
                    void const* lhs, TRhs&& rhs)
            {
                if (type != typeid(T))
                    return TryAdd<TRet, TRhs, Ts...>()(type, lhs,
                            std::forward<TRhs>(rhs));

                return *static_cast<T const*>(lhs) + std::forward<TRhs>(rhs);
            }
        };

        template <typename TRet, typename TRhs, typename T>
        struct TryAdd<TRet, TRhs, T>
        {
            TRet operator()(std::type_index const& type,
                    void const* lhs, TRhs&& rhs)
            {
                (void) type;
                assert(type == typeid(T));

                return *static_cast<T const*>(lhs) + std::forward<TRhs>(rhs);
            }
        };

        /*template <typename T1, typename T2>
        using multiply_t = decltype(std::declval<T1>() * std::declval<T2>());*/

        template <typename TLhs, typename TRhs, typename = void>
        struct CanMultiply : std::false_type {};

        template <typename TLhs, typename TRhs>
        struct CanMultiply<TLhs, TRhs, VoidType<multiply_t<TLhs, TRhs>>> :
            std::true_type {};

        template <typename TRhs, typename TLhs, typename... TLhss>
        struct AllCanMultiply
        {
            static const bool value = CanMultiply<TLhs, TRhs>::value
                && AllCanMultiply<TRhs, TLhss...>::value;
        };

        template <typename TRhs, typename TLhs>
        struct AllCanMultiply<TRhs, TLhs>
        {
            static const bool value = CanMultiply<TLhs, TRhs>::value;
        };

        template <typename TRet, typename TRhs, typename T, typename... Ts>
        struct TryMultiply
        {
            TRet operator()(std::type_index const& type,
                    void const* lhs, TRhs&& rhs)
            {
                if (type != typeid(T))
                    return TryMultiply<TRet, TRhs, Ts...>()(type, lhs,
                            std::forward<TRhs>(rhs));

                return *static_cast<T const*>(lhs) * std::forward<TRhs>(rhs);
            }
        };

        template <typename TRet, typename TRhs, typename T>
        struct TryMultiply<TRet, TRhs, T>
        {
            TRet operator()(std::type_index const& type,
                    void const* lhs, TRhs&& rhs)
            {
                (void) type;
                assert(type == typeid(T));

                return *static_cast<T const*>(lhs) * std::forward<TRhs>(rhs);
            }
        };

        template <typename T>
        constexpr auto max(T const& t) -> T const&
        {
            return t;
        }

        template <typename T, typename... Ts>
        constexpr auto max(T const& t, Ts const&... ts)
        //-> decltype(std::max(t, max(ts...)))
        {
            return std::max(t, max(ts...));
        }
    } // namespace detail

    template <typename... Ts>
    class alignas(detail::max(8lu, alignof(Ts)...)) variant
    {
    public:
        template <typename T, typename =
            typename btl::enable_if_t<IsOneOf<btl::decay_t<T>,
                     Ts...>::value>>
        variant(T&& value) :
            type_(typeid(T))
        {
            new (data_)btl::decay_t<T>(std::forward<T>(value));
        }

        variant(variant<Ts...> const& rhs) :
            type_(rhs.type_)
        {
            detail::TryCopyAs<Ts...>()(rhs.type_, data_, rhs.data_);
        }

        template <typename... Tr, typename = typename btl::enable_if_t<
            detail::IsAssignableTo<variant<Ts...>, variant<Tr...>>::value>>
        variant(variant<Tr...> const& rhs) :
            type_(rhs.type_)
        {
            detail::TryCopyAs<Ts...>()(rhs.type_, data_, rhs.data_);
        }

        variant(variant<Ts...>&& rhs) noexcept :
            type_(rhs.type_)
        {
            detail::TryMoveAs<Ts...>()(rhs.type_, data_, rhs.data_);
        }

        template <typename... Tr, typename = typename btl::enable_if_t<
            detail::IsAssignableTo<variant<Ts...>, variant<Tr...>>::value>>
        variant(variant<Tr...>&& rhs)
        noexcept(detail::AreNothrowMoveConstructible<Ts...>::value) :
            type_(rhs.type_)
        {
            detail::TryMoveAs<Ts...>()(rhs.type_, data_, rhs.data_);
        }

        ~variant()
        {
            detail::TryDeconstructAs<Ts...>()(type_, data_);
        }

        template <typename T, typename =
            typename btl::enable_if_t<IsOneOf<T, Ts...>::value>>
        T& get()
        {
            assert(is<T>());
            void* p = data_;
            return *(btl::decay_t<T>*)(p);
        }

        template <typename T, typename =
            typename btl::enable_if_t<IsOneOf<T, Ts...>::value>>
        T const& get() const
        {
            assert(is<T>());
            void const* p = data_;
            return *(btl::decay_t<T> const*)(p);
        }

        template <typename T>
        bool is() const
        {
            return type_ == typeid(T);
        }

        variant<Ts...>& operator=(variant<Ts...> const& rhs)
        {
            detail::TryDeconstructAs<Ts...>()(type_, data_);
            detail::TryCopyAs<Ts...>()(rhs.type_, data_, rhs.data_);
            type_ = rhs.type_;
            return *this;
        };

        template <typename... Tr, typename = typename btl::enable_if_t<
            detail::IsAssignableTo<variant<Ts...>, variant<Tr...>>::value>>
        variant<Ts...>& operator=(variant<Tr...> const& rhs)
        {
            detail::TryDeconstructAs<Ts...>()(type_, data_);
            detail::TryCopyAs<Ts...>()(rhs.type_, data_, rhs.data_);
            type_ = rhs.type_;
            return *this;
        }

        variant<Ts...>& operator=(variant<Ts...>&& rhs) noexcept
        {
            detail::TryDeconstructAs<Ts...>()(type_, data_);
            detail::TryMoveAs<Ts...>()(rhs.type_, data_, rhs.data_);
            type_ = rhs.type_;
            return *this;
        }

        template <typename... Tr, typename = typename btl::enable_if_t<
                detail::IsAssignableTo<variant<Ts...>, variant<Tr...>>::value>>
        variant<Ts...>& operator=(variant<Tr...>&& rhs) noexcept(
                detail::AreNothrowMoveConstructible<Tr...>::value)
        {
            detail::TryDeconstructAs<Ts...>()(type_, data_);
            detail::TryMoveAs<Ts...>()(rhs.type_, data_, rhs.data_);
            type_ = rhs.type_;
            return *this;
        }

        template <typename T, typename =
            typename btl::enable_if_t<IsOneOf<T, Ts...>::value>>
        variant& operator=(T&& value)
        {
            detail::TryDeconstructAs<Ts...>()(type_, data_);
            new (data_)btl::decay_t<T>(std::forward<T>(value));
            type_ = typeid(T);
            return *this;
        }

        template <typename T>
            btl::enable_if_t<IsOneOf<T, Ts...>::value, bool>
        operator==(T&& rhs) const noexcept
        {
            if (type_ != typeid(T))
                return false;

            return detail::TryCompareEqual<Ts...>()(type_, data_, &rhs);
        }

        bool operator==(variant<Ts...> const& rhs) const noexcept
        {
            if (type_ != rhs.type_)
                return false;

            return detail::TryCompareEqual<Ts...>()(type_, data_, rhs.data_);
        }

        template <typename... Tr>
        bool operator==(variant<Tr...> const& rhs) const noexcept
        {
            if (type_ != rhs.type_)
                return false;

            return detail::TryCompareEqual<Ts...>()(type_, data_, rhs.data_);
        }

        template <typename T>
        btl::enable_if_t<detail::AllCanAdd<T, Ts...>::value,
        variant<Ts...>>
            operator+(T&& rhs) const
        {
            return detail::TryAdd<variant<Ts...>, T, Ts...>()(type_, data_,
                    std::forward<T>(rhs));
        }

        template <typename T>
        btl::enable_if_t<detail::AllCanMultiply<T, Ts...>::value,
        variant<Ts...>> operator*(T&& rhs) const
        {
            return detail::TryMultiply<variant<Ts...>, T, Ts...>()(type_,
                    data_, std::forward<T>(rhs));
        }

        template <typename T, typename TFunc, typename =
            typename btl::enable_if_t<IsOneOf<T, Ts...>::value>>
        variant<Ts...>& match(TFunc&& f)
        {
            if (type_ == typeid(T))
                f(get<T>());

            return *this;
        }

        template <typename T, typename TFunc, typename =
            typename btl::enable_if_t<IsOneOf<T, Ts...>::value>>
        variant<Ts...> const& match(TFunc&& f) const
        {
            if (type_ == typeid(T))
                f(get<T>());

            return *this;
        }

        template <class THash>
        friend void hash_append(THash& h, variant<Ts...> const& u) noexcept
        {
            using btl::hash_append;
            detail::TryHashAs<THash, Ts...>()(h, u.type_, u.data_);
        }

    private:
        template <typename... T> friend class variant;
        std::type_index type_;
        char data_[detail::MaxSizeOf<Ts...>::value];
    };

    static_assert(std::is_nothrow_move_constructible<
            variant<int, std::string>>::value, "");
    static_assert(std::is_nothrow_move_assignable<
            variant<int, std::string>>::value, "");
}

