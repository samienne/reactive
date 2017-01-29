#pragma once

#include "future/future.h"

#include "threadpool.h"
#include "apply.h"

namespace btl
{
    template <typename TFunc, typename... Ts,
             typename = std::result_of_t<TFunc(Ts...)>>
    void delayedJob(ThreadPool::Duration delay, TFunc&& func, Ts&&... ts)
    {
        ThreadPool::getInstance().delayed(
                delay,
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    template <typename TFunc, typename... Ts>
    auto delayed(ThreadPool::Duration delay, TFunc&& f, Ts&&... ts)
        -> future::Future<std::decay_t<std::result_of_t<TFunc(Ts...)>>>
    {
        using ValueType = std::decay_t<std::result_of_t<TFunc(Ts...)>>;

        auto control = std::make_shared<future::FutureControl<ValueType>>();
        future::Promise<ValueType> promise(control);
        future::Future<ValueType> future(std::move(control));
        auto params = std::make_tuple(std::forward<Ts>(ts)...);

        btl::delayedJob(
                delay,
                [
                promise=std::move(promise),
                func=std::forward<TFunc>(f),
                params=std::move(params)
                ]
                () mutable
        {
            if (!promise.valid())
                return;

            promise.set(btl::apply(std::move(func), std::move(params)));
        });

        return future;
    }
} // btl

