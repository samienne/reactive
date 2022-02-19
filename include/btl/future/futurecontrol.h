#pragma once

#include "traits.h"
#include "futurebase.h"

#include "../threadpool.h"

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <tuple>
#include <utility>

namespace btl::future
{
    template <typename... Ts>
    class FutureControl : public FutureBase
    {
        using LockType = std::unique_lock<SpinLock>;

    public:
        FutureControl() :
            ready_(false)
        {
        }

        void set(std::tuple<Ts...> value)
        {
            assert(!ready());

            LockType lock(mutex_);
            value_ = std::make_optional(std::move(value));
            TSAN_ANNOTATE_HAPPENS_BEFORE(&value_);
            TSAN_ANNOTATE_HAPPENS_BEFORE(&value_.getReferenceForTsan());
            ready_.store(true, std::memory_order_release);

            if (callback_.has_value())
            {
                auto callback = std::move(*callback_);
                lock.unlock();

                callback();
            }
        }

        void waitForResult()
        {
            // Fast preliminary check if we don't have to wait.
            if (ready())
                return;

            LockType lock(mutex_);

            // Second check after actually locking the mutex just to be sure.
            if (value_.has_value())
                return;

            //assert(!callback_);

            std::mutex mutex;
            std::condition_variable condition;

            auto oldCallback = std::move(callback_);
            callback_ = std::make_optional<MoveOnlyFunction<void()>>(
                    [&condition, &mutex]() mutable
            {
                std::unique_lock<std::mutex> lock2(mutex);
                condition.notify_one();
            });

            std::unique_lock<std::mutex> lock2(mutex);

            lock.unlock();
            condition.wait(lock2);
            if (oldCallback.has_value())
                (*oldCallback)();
        }

        /*
        template <int... S>
        std::tuple<std::decay_t<Ts> const&...> getTupleRefImpl(std::index_sequence<S...>) const&
        {
            assert(value_.has_value());

            return std::make_tuple<std::decay_t<Ts> const&...>(
                    std::get<S>(*value_)...
                    );
        }

        std::tuple<std::decay_t<Ts> const&...> getTupleRef() const&
        {
        }
        */

        //std::tuple<Ts...> get()
        /*
        auto get()
        {
            waitForResult();

            assert(value_.has_value());

            // No need for lock as the value will be set only once
            // and we know that it has been set already. Also we are
            // the only consumer for the value.
            if constexpr(sizeof...(Ts) == 0)
                return;
            else if constexpr(sizeof...(Ts) == 1)
                return std::move(std::get<0>(*value_));
            else
                return getTuple();
        }
        */

        std::tuple<Ts...> getTuple()
        {
            assert(value_.has_value());

            return *value_;
        }

        template <typename... Us, size_t... S>
        auto getAsTupleImpl(std::index_sequence<S...>) -> decltype(auto)
        {
            assert(value_.has_value());

            return std::make_tuple<Us...>(
                    (Us)std::get<S>(*value_)...
                    );
        }

        template <typename... Us>
        auto getAsTuple()
        {
            assert(value_.has_value());

            return getAsTupleImpl<Us...>(std::make_index_sequence<sizeof...(Us)>());
        }

        template <typename U>
        auto getFirst() -> U
        {
            assert(value_.has_value());
            return std::move(std::get<0>(*value_));
        }

        template <typename... Us>
        auto get() -> decltype(auto)
        {
            static_assert(sizeof...(Ts) == sizeof...(Us));
            waitForResult();

            assert(value_.has_value());

            // No need for lock as the value will be set only once
            // and we know that it has been set already. Also we are
            // the only consumer for the value.
            if constexpr(sizeof...(Ts) == 0)
                return;
            else if constexpr(sizeof...(Ts) == 1)
                return getFirst<Us...>();
            else
                return getAsTuple<Us...>();
        }

        //std::tuple<std::decay_t<Ts> const&...> getRef()
        /*
        auto getRef()
        {
            waitForResult();

            assert(value_.has_value());

            // No need for lock as the value will be set only once
            // and we know that it has been set already.
            if constexpr(sizeof...(Ts) == 0)
                return;
            else if constexpr(sizeof...(Ts) == 1)
                return std::get<0>(*value_);
            else
                return getTupleRef();
        }
        */

        bool ready() const
        {
            bool r = ready_.load(std::memory_order_acquire);
            TSAN_ANNOTATE_HAPPENS_AFTER(&value_);
            TSAN_ANNOTATE_HAPPENS_AFTER(&value_.getReferenceForTsan());
            return r;
        }

        void addCallback(btl::MoveOnlyFunction<void()> callback)
        {
            if (ready())
            {
                callback();
                return;
            }

            LockType lock(mutex_);

            // Check if the value was set between calling ready() and
            // locking the mutex..
            if (value_.has_value())
            {
                lock.unlock();
                callback();
                return;
            }

            if (callback_.has_value())
            {
                callback_ = std::make_optional<MoveOnlyFunction<void()>>(
                        [oldCb = std::move(callback_),
                            cb=std::move(callback)]() mutable
                {
                    std::move(*oldCb)();
                    std::move(cb)();
                });
            }
            else
                callback_ = std::make_optional(std::move(callback));
        }

    private:
        SpinLock mutex_;
        std::atomic<bool> ready_;
        std::optional<std::tuple<Ts...>> value_;
        std::optional<btl::MoveOnlyFunction<void()>> callback_;
    };
} // namespace btl::future

