#pragma once

#include "future/future.h"

#include "threadpool.h"

namespace btl
{
    template <typename TFunc, typename... Ts,
             typename = std::invoke_result_t<TFunc, Ts...>>
    void asyncJob(TFunc&& func, Ts&&... ts)
    {
        ThreadPool::getInstance().async(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...);
    }

    template <typename TFunc, typename... Ts>
    auto async(TFunc&& f, Ts&&... ts)
        -> future::Future<std::decay_t<std::invoke_result_t<TFunc, Ts...>>>
    {
        using ValueType = std::decay_t<std::invoke_result_t<TFunc, Ts...>>;

        auto control = std::make_shared<future::FutureControl<ValueType>>();
        future::Promise<ValueType> promise(control);
        future::Future<ValueType> future(std::move(control));
        auto params = std::make_tuple(std::forward<Ts>(ts)...);

        btl::asyncJob(
                [
                promise=std::move(promise),
                func=std::forward<TFunc>(f),
                params=std::move(params)
                ]
                () mutable
        {
            if (!promise.valid())
                return;

            promise.set(std::apply(std::move(func), std::move(params)));
        });

        return future;
    }
} // btl

