#pragma once

#include <btl/moveonlyfunction.h>
#include <btl/spinlock.h>

#include <btl/tsan.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

namespace btl::future
{
    template <typename T>
    using FutureType = decltype(std::declval<std::decay_t<T>>().get());

    template <typename... Ts>
    class Future;

    template <typename... Ts>
    class SharedFuture;

    template <typename T>
    struct FutureValueType
    {
        using type = T;
    };

    template <typename T>
    struct FutureValueType<Future<T>>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    struct FutureValueType<Future<T>&>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    struct FutureValueType<Future<T> const&>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    struct FutureValueType<Future<T>&&>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    struct FutureValueType<SharedFuture<T>>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    struct FutureValueType<SharedFuture<T>&>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    struct FutureValueType<SharedFuture<T> const &>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    struct FutureValueType<SharedFuture<T>&&>
    {
        using type = typename FutureValueType<T>::type;
    };

    template <typename T>
    using FutureValueTypeT = typename FutureValueType<T>::type;

    class FutureBase : public std::enable_shared_from_this<FutureBase>
    {
    public:
        virtual ~FutureBase() = default;
    };
} // namespace btl::future

