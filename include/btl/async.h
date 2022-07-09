#pragma once

#include "future/promise.h"
#include "future/future.h"

#include "threadpool.h"
#include <exception>

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
        //-> future::Future<std::decay_t<std::invoke_result_t<TFunc, Ts...>>>
    {
        using ValueType = std::decay_t<std::invoke_result_t<TFunc, Ts...>>;

        using ControlType = std::conditional_t<
            future::IsFutureResult<ValueType>::value,
            future::ApplyParamsFromT<ValueType, future::FutureControl>,
            std::conditional_t<
                std::is_same_v<void, ValueType>,
                future::FutureControl<>,
                future::FutureControl<ValueType>
            >>;

        using FutureType = future::ApplyParamsFromT<ControlType, future::Future>;

        using PromiseType = future::ApplyParamsFromT<FutureType, future::Promise>;

        auto control = std::make_shared<ControlType>();
        PromiseType promise(control);
        FutureType future(std::move(control));
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

            try
            {
                if constexpr (future::IsFutureResult<ValueType>::value)
                {
                    promise.setFromTuple(std::apply(
                                std::move(func),
                                std::move(params)
                                ).getAsTuple()
                            );
                }
                else if constexpr (std::is_void_v<ValueType>)
                {
                    std::apply(std::move(func), std::move(params));
                    promise.set();
                }
                else
                {
                    promise.set(std::apply(std::move(func), std::move(params)));
                }
            }
            catch(...)
            {
                promise.setFailure(std::current_exception());
            }
        });

        return future;
    }
} // btl

