//
// Original Copyright Tomasz Kamiński 2013 - 2014.
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file ../LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified under same licence by Sami Enne

#pragma once

#include <type_traits>
#include <utility>

namespace btl
{

    namespace type_traits
    {
        template<typename T>
        struct target_type
        {
            typedef void type;
        };

        template<typename Class, typename Member>
        struct target_type<Member Class::*>
        {
            typedef Class type;
        };

        //Is reference to pointer target or derived
        template<typename Object, typename Pointer>
        struct is_target_reference :
            public std::integral_constant<
            bool,
            std::is_reference<Object>::value &&
            std::is_base_of<
            typename target_type<Pointer>::type,
            typename std::decay<Object>::type
            >::value
            >
        {};

        namespace detail
        {
            //MPL or
            constexpr bool predicate_or()
            {
                return false;
            }

            template<typename Pred, typename... Preds>
            constexpr bool predicate_or(Pred&& pred, Preds&&... preds)
            {
                return pred || predicate_or(preds...);
            }

            template<typename Object, typename TargetType>
            struct is_wrapper_compatible_with_member_pointer_impl
            : std::integral_constant<bool, predicate_or(
                    std::is_convertible<Object, TargetType&>{},
                    std::is_convertible<Object, TargetType const&>{},
                    std::is_convertible<Object, TargetType&&>{},
                    std::is_convertible<Object, TargetType const&&>{})>
            {};

            template<typename Object>
            struct is_wrapper_compatible_with_member_pointer_impl<Object, void>
            : std::false_type
            {};
        }

        template<typename Object, typename Pointer>
        struct is_wrapper_compatible_with_member_pointer
        : detail::is_wrapper_compatible_with_member_pointer_impl<Object,
        typename target_type<Pointer>::type>
        {};

        namespace detail
        {
            template<typename Object, typename Pointer>
            constexpr bool is_pointer_compatible_with_member_pointer_impl(
                    typename std::decay<decltype(*std::declval<Object>())>::type*)
            {
                return is_wrapper_compatible_with_member_pointer<decltype(
                        *std::declval<Object>()), Pointer>::value;
            }

            template<typename Object, typename Pointer>
            constexpr bool is_pointer_compatible_with_member_pointer_impl(...)
            {
                return false;
            }
        }

        template<typename Object, typename Pointer>
        struct is_pointer_compatible_with_member_pointer :
            public std::integral_constant<bool,
            detail::is_pointer_compatible_with_member_pointer_impl<
            Object, Pointer>(0)>
        {};
    } // type_traits

    namespace functional
    {
        template<typename Functor, typename Object, typename... Args>
        constexpr auto invoke(Functor&& functor, Object&& object, Args&&... args)
        ->  typename std::enable_if<
        std::is_member_function_pointer<
        typename std::decay<Functor>::type
        >::value &&
        type_traits::is_target_reference<
        Object&&,
        typename std::decay<Functor>::type
            >::value,
        decltype((std::forward<Object>(object).*functor)(std::forward<Args>(args)...))
            >::type
            {
                return (std::forward<Object>(object).*functor)(std::forward<Args>(args)...);
            }

        template<typename Functor, typename Object, typename... Args>
        constexpr auto invoke(Functor&& functor, Object&& object, Args&&... args)
        ->  typename std::enable_if<
        std::is_member_function_pointer<
        typename std::decay<Functor>::type
        >::value &&
        !type_traits::is_target_reference<
        Object&&,
        typename std::decay<Functor>::type
            >::value,
        decltype(((*std::forward<Object>(object)).*functor)(std::forward<Args>(args)...))
            >::type
            {
                return ((*std::forward<Object>(object)).*functor)(std::forward<Args>(args)...);
            }

        template<typename Functor, typename Object>
        constexpr auto invoke(Functor&& functor, Object&& object)
        ->  typename std::enable_if<
        std::is_member_object_pointer<
        typename std::decay<Functor>::type
        >::value &&
        type_traits::is_target_reference<
        Object&&,
        typename std::decay<Functor>::type
            >::value,
        decltype(std::forward<Object>(object).*functor)
            >::type
            {
                return std::forward<Object>(object).*functor;
            }

        template<typename Functor, typename Object>
        constexpr auto invoke(Functor&& functor, Object&& object)
        ->  typename std::enable_if<
        std::is_member_object_pointer<
        typename std::decay<Functor>::type
        >::value &&
        !type_traits::is_target_reference<
        Object&&,
        typename std::decay<Functor>::type
            >::value,
        decltype((*std::forward<Object>(object)).*functor)
            >::type
            {
                return (*std::forward<Object>(object)).*functor;
            }

        template<typename Functor, typename... Args>
        constexpr auto invoke(Functor&& functor, Args&&... args)
        ->  typename std::enable_if<
        !std::is_member_pointer<
        typename std::decay<Functor>::type
        >::value,
        decltype(std::forward<Functor>(functor)(std::forward<Args>(args)...))
            >::type
            {
                return std::forward<Functor>(functor)(std::forward<Args>(args)...);
            }
    } // functional

    template <typename TFunc, typename... TArgs>
    constexpr auto invoke(TFunc&& func, TArgs&&... args)
        -> decltype(
                functional::invoke(
                    std::forward<TFunc>(func),
                    std::forward<TArgs>(args)...
                    )
                )
    {
        return functional::invoke(
                std::forward<TFunc>(func),
                std::forward<TArgs>(args)...
                );
    }

} // btl

