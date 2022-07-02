#pragma once

#include "future.h"

#include <btl/tupleforeach.h>

#include <type_traits>
#include <utility>

namespace btl::future
{
    namespace detail
    {
        template <typename T, typename TFunc>
        auto waitForValue(T&& t, TFunc&& func)
        {
            if constexpr(IsFuture<std::decay_t<T>>::value)
            {
                t.addCallback_([func=std::forward<TFunc>(func)]() mutable
                {
                    std::invoke(std::move(func));
                });
            }
        }

        template <typename T>
        auto getValueAsTuple(T&& t)
        {
            if constexpr(IsFuture<std::decay_t<T>>::value)
            {
                return std::forward<T>(t).getTuple();
            }
            else
            {
                return std::make_tuple(std::forward<T>(t));
            }
        }
    } // namespace detail

    template <typename... Ts>
    class WhenAll : public FutureControl<std::decay_t<FutureType<Ts>>...>
    {
    public:
        WhenAll(std::tuple<Ts...> values) :
            count_(((IsFuture<std::decay_t<Ts>>::value ? 1 : 0) + ... + 0)),
            values_(std::move(values))
        {
        }

        void init()
        {
            auto control = this->weak_from_this();

            btl::tuple_foreach(values_, [&control](auto&& value)
            {
                detail::waitForValue(std::forward<decltype(value)>(value),
                    [control]() mutable
                    {
                        if (auto p = control.lock())
                        {
                            auto ptr = static_cast<WhenAll*>(p.get());
                            ptr->reportValueReady();
                        }
                    });
            });
        }

        void reportValueReady()
        {
            auto count = count_.fetch_sub(1);

            assert(count >= 1);

            if (count == 1)
            {
                std::apply([this](auto&&... values)
                    {
                        this->set(std::tuple_cat(
                                    detail::getValueAsTuple(
                                        std::forward<decltype(values)>(values)
                                        )...
                                    ));
                    },
                    std::move(values_)
                    );
            }
        }

    private:
        std::atomic_int count_;
        std::tuple<Ts...> values_;
    };

    template <typename... Ts>
    auto whenAll(Ts&&... ts) -> Future<FutureType<std::decay_t<Ts>>...>
    {
        constexpr size_t futureCount = ((IsFuture<std::decay_t<Ts>>::value ? 1 : 0) + ... + 0);

        if constexpr (futureCount == 0)
            return makeReadyFuture<FutureType<Ts>...>(std::forward<Ts>(ts)...);

        auto control = std::make_shared<WhenAll<std::decay_t<Ts>...>>(
                std::make_tuple(std::forward<Ts>(ts)...)
                );

        control->init();

        return Future<FutureType<std::decay_t<Ts>>...>(
                std::move(control)
                );
    }
} // namespace btl::future

