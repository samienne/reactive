#pragma once

#include "../typetraits.h"

#include <utility>
#include <future>

namespace btl
{
    namespace future
    {
        template <typename T>
        auto get(T&& future) -> decltype(future.get())
        {
            return std::forward<T>(future).get();
        }

        template <typename T>
        auto isReady(T&& future) -> decltype(future.isReady())
        {
            return std::forward<T>(future).isReady();
        }

        template <typename T>
        auto get(std::future<T> const& future) -> decltype(future.get())
        {
            return future.get();
        }

        template <typename T>
        bool isReady(std::future<T> const& future)
        {
            return future.valid();
        }
    } // future

    namespace detail
    {
        template <typename T>
        using GetType = decltype(future::get(std::declval<T>()));

        template <typename T>
        using IsReadyType = decltype(future::isReady(std::declval<T const>()));
    } // detail

    template <typename T, typename = void>
    struct IsFuture : std::false_type {};

    template <typename T>
    struct IsFuture<T, void_t
        <
            detail::GetType<typename std::decay<T>::type>,
            detail::IsReadyType<typename std::decay<T>::type>
        >
    > : std::true_type {};
} // btl

