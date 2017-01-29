#pragma once

#include "futurebase.h"

#include <btl/tuplelast.h>
#include <btl/tupleallbutlast.h>
#include <btl/tuplereverse.h>

#include <btl/fmap.h>
#include <btl/tupleforeach.h>
#include <btl/tuplemap.h>
#include <btl/all.h>
#include <btl/not.h>

namespace btl
{
    namespace future
    {
        template <typename T>
        class Future;

        template <typename T>
        class SharedFuture;

        template <typename T>
        Future<T> toFuture(Future<T> t)
        {
            return std::move(t);
        }

        template <typename T>
        auto toFuture(SharedFuture<T> t)
        {
            return Future<T const&>(std::move(t));
        }

        template <typename TFunc, typename... Ts>
        class MappedFuture final :
            public FutureControl<std::decay_t<std::result_of_t<TFunc(Ts...)>>>
        {
        public:
            MappedFuture(TFunc&& func, std::tuple<Future<Ts>...> futures) :
                count_(sizeof...(Ts)),
                futures_(std::move(futures)),
                func_(std::forward<TFunc>(func))
            {
            }

            void init()
            {
                using ValueType = std::result_of_t<TFunc(Ts...)>;
                std::weak_ptr<FutureControl<ValueType>> control =
                    std::static_pointer_cast<FutureControl<ValueType>>(
                            this->shared_from_this());

                btl::tuple_foreach(futures_, [&control](auto&& future) mutable
                {
                    future.addCallback_([control]() mutable
                    {
                        if (auto p = control.lock())
                        {
                            auto* ptr =
                                static_cast<MappedFuture<TFunc, Ts...>*>(
                                    p.get());

                            ptr->reportFutureReady();
                        }
                    });

                });
            }

            void reportFutureReady()
            {
                auto count = count_.fetch_sub(1);

                assert(count >= 1);

                if (count == 1)
                {
                    this->set(btl::apply(
                            std::move(func_),
                            btl::tuple_map(
                                std::move(futures_),
                                [](auto&& future)
                                {
                                    return std::move(future).get();
                                }
                            )
                        )
                    );
                }
            }

        private:
            std::atomic_int count_;
            std::tuple<Future<Ts>...> futures_;
            std::decay_t<TFunc> func_;
        };

        template <typename T>
        using FutureType = decltype(std::declval<T>().get());

        template <typename T, typename = void>
        struct IsFuture : std::false_type {};

        template <typename T>
        struct IsFuture<T, std::enable_if_t<
            std::is_convertible<T, Future<FutureType<T>>>::value
            >
        > : std::true_type {};

        static_assert(!IsFuture<int>::value, "");

        template <typename T>
        using IsViableFMapParam =
        std::conditional_t
            <
                std::is_reference<T>::value,
                std::is_copy_constructible<std::decay_t<T>>,
                std::true_type
            >;

        template <typename TFunc, typename... TFutures, typename =
            std::enable_if_t
            <
                All
                <
                    IsViableFMapParam<FutureType<TFutures>>...,
                    btl::Not<std::is_same<
                        void,
                        std::result_of_t<TFunc(FutureType<TFutures>...)>
                    >>
                >::value
            >>
        auto fmap(TFunc&& func, TFutures&&... futures)
        -> Future<std::result_of_t<TFunc(FutureType<TFutures>...)>>
        {
            using ReturnValueType = std::result_of_t<TFunc(FutureType<TFutures>...)>;
            using ControlType = MappedFuture<TFunc, FutureType<TFutures>...>;

            auto tuple = std::make_tuple(std::move(futures)...);

            auto control = std::make_shared<ControlType>(
                    std::forward<TFunc>(func),
                    std::move(tuple)
                    );

            control->init();

            return Future<ReturnValueType>(std::move(control));
        }

        template <typename... Ts>
        auto fmap2(Ts&&... ts)
        {
            auto t = std::forward_as_tuple(std::forward<Ts>(ts)...);
            return btl::apply([&](auto&&... params)
                    {
                        return fmap(
                            tuple_last(std::move(t)),
                            std::forward<decltype(params)>(params)...
                            );
                    },
                    tuple_all_but_last(std::move(t))
                    );
        }
    } // future

    template <typename... Ts>
    auto fmap(Ts&&... ts)
    -> decltype(
            future::fmap(std::forward<Ts>(ts)...)
        )
    {
        return future::fmap(std::forward<Ts>(ts)...);
    }
} // btl

