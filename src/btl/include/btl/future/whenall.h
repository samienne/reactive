#pragma once

#include "future.h"

#include <btl/spinlock.h>
#include <btl/tupleforeach.h>
#include <btl/typelist.h>

#include <atomic>
#include <exception>
#include <mutex>
#include <optional>
#include <tuple>
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
                t.addCallback_([func=std::forward<TFunc>(func)](auto& control) mutable
                {
                    std::invoke(std::move(func), control);
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
    class WhenAll : public //FutureControl<std::decay_t<FutureType<Ts>>...>
                    ApplyParamsFromT<
                    typename  btl::FilterOut<
                    std::is_void,
                    btl::TypeList<std::decay_t<FutureType<Ts>>...>
                    >::type,
                    FutureControl
                    >
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

            btl::tuple_foreach(*values_, [this, &control](auto&& value)
            {
                if (failed_.load(std::memory_order_acquire))
                    return;

                detail::waitForValue(std::forward<decltype(value)>(value),
                    [control](auto& valueControl) mutable
                    {
                        if (auto p = control.lock())
                        {
                            auto ptr = static_cast<WhenAll*>(p.get());

                            if (valueControl.hasValue())
                                ptr->reportValueReady();
                            else
                                ptr->reportFailure(valueControl.getException());
                        }
                    });

            });

            // Publish that registration is complete and, under the same lock a
            // failing sibling uses, drop values_ if a failure has already been
            // reported. Serialising the initialized_/values_ pair here (rather
            // than handing them off through two independent atomics) closes the
            // store-buffering window where init() and reportFailure() could
            // each observe the other's flag as unset and *both* skip the reset,
            // leaving a cancelled future's continuation to run.
            std::unique_lock<SpinLock> lock(valuesMutex_);
            initialized_ = true;
            if (failed_.load(std::memory_order_acquire))
                values_.reset();
        }

        void reportFailure(std::exception_ptr err)
        {
            bool expected = false;
            bool hadNotFailed = failed_.compare_exchange_strong(expected, true);
            if (hadNotFailed)
            {
                this->setFailure(std::move(err));

                std::unique_lock<SpinLock> lock(valuesMutex_);
                if (initialized_)
                    values_.reset();
            }
        }

        void reportValueReady()
        {
            auto count = count_.fetch_sub(1);

            assert(count >= 1);

            if (count == 1)
            {
                std::unique_lock<SpinLock> lock(valuesMutex_);

                if (!values_)
                    return;

                std::apply([this](auto&&... values)
                    {
                        this->setValue(std::tuple_cat(
                                    detail::getValueAsTuple(
                                        std::forward<decltype(values)>(values)
                                        )...
                                    ));
                    },
                    std::move(*values_)
                    );

                values_.reset();
            }
        }

    private:
        // Serialises every access to values_ so exactly one thread ever drops
        // it: init() completing, a sibling reporting failure, or the final
        // value becoming ready. Without this the failed_/initialized_ handshake
        // is a store-buffering race (both loads can miss on ARM64).
        SpinLock valuesMutex_;
        std::atomic_int count_;
        std::atomic_bool failed_ = false;
        bool initialized_ = false;
        std::optional<std::tuple<Ts...>> values_;
    };

    template <typename... Ts>
    auto whenAll(Ts&&... ts) // -> Future<FutureType<std::decay_t<Ts>>...>
    {
        constexpr size_t futureCount = ((IsFuture<std::decay_t<Ts>>::value ? 1 : 0) + ... + 0);

        if constexpr (futureCount == 0)
            return makeReadyFuture<FutureType<Ts>...>(std::forward<Ts>(ts)...);

        auto control = std::make_shared<WhenAll<std::decay_t<Ts>...>>(
                std::make_tuple(std::forward<Ts>(ts)...)
                );

        control->init();

        using Types = typename btl::FilterOut<
            std::is_void,
            btl::TypeList<FutureType<std::decay_t<Ts>>...>
            >::type;

        using ResultType = ApplyParamsFromT<Types, Future>;

        return ResultType(std::move(control));
    }
} // namespace btl::future

