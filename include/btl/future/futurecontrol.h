#pragma once

#include "traits.h"
#include "futurebase.h"

#include "../threadpool.h"

#include <atomic>
#include <exception>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

namespace btl::future
{
    template <typename... Ts>
    class FutureControl : public FutureBase
    {
        using LockType = std::unique_lock<SpinLock>;

    public:
        FutureControl() :
            ready_(false),
            value_(std::monostate{})
        {
        }

        void setValue(std::tuple<Ts...> value)
        {
            set(std::move(value));
        }

        void setFailure(std::exception_ptr err)
        {
            set(std::move(err));
        }

        void waitForResult()
        {
            // Fast preliminary check if we don't have to wait.
            if (ready())
                return;

            LockType lock(mutex_);

            ready_.load(std::memory_order_acquire);

            // Second check after actually locking the mutex just to be sure.
            if (!std::holds_alternative<std::monostate>(value_))
                return;

            std::mutex mutex;
            std::condition_variable condition;

            auto oldCallback = std::move(callback_);
            callback_ = std::make_optional<MoveOnlyFunction<void(FutureControl&)>>(
                    [&condition, &mutex](FutureControl&) mutable
            {
                std::unique_lock<std::mutex> lock2(mutex);
                condition.notify_one();
            });

            std::unique_lock<std::mutex> lock2(mutex);

            lock.unlock();
            condition.wait(lock2);

            ready_.load(std::memory_order_acquire);

            if (oldCallback.has_value())
                (*oldCallback)(*this);
        }

        std::tuple<Ts...> const& getTupleRef() const
        {
            waitForResult();

            assert(!std::holds_alternative<std::monostate>(value_));

            if (std::holds_alternative<std::exception_ptr>(value_))
                std::rethrow_exception(std::get<std::exception_ptr>(value_));

            return std::get<std::tuple<Ts...>>(value_);
        }

        std::tuple<Ts...>& getTupleRef()
        {
            waitForResult();

            assert(!std::holds_alternative<std::monostate>(value_));

            if (std::holds_alternative<std::exception_ptr>(value_))
                std::rethrow_exception(std::get<std::exception_ptr>(value_));

            return std::get<std::tuple<Ts...>>(value_);
        }

        std::tuple<Ts...> getTuple()
        {
            return getTupleRef();
        }

        template <typename... Us, size_t... S>
        auto getAsTupleImpl(std::index_sequence<S...>) -> decltype(auto)
        {
            auto&& value = getTupleRef();

            return std::make_tuple<Us...>(
                    (Us)std::get<S>(value)...
                    );
        }

        template <typename... Us>
        auto getAsTuple()
        {
            return getAsTupleImpl<Us...>(std::make_index_sequence<sizeof...(Us)>());
        }

        template <typename U>
        auto getFirst() -> U
        {
            return std::move(std::get<0>(getTupleRef()));
        }

        template <typename... Us>
        auto get() -> decltype(auto)
        {
            static_assert(sizeof...(Ts) == sizeof...(Us));

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

        std::exception_ptr getException()
        {
            bool r = ready();
            assert(r && hasException());

            return std::get<std::exception_ptr>(value_);
        }

        bool ready() const
        {
            bool r = ready_.load(std::memory_order_acquire);
            TSAN_ANNOTATE_HAPPENS_AFTER(&value_);
            TSAN_ANNOTATE_HAPPENS_AFTER(&value_.getReferenceForTsan());
            return r;
        }

        bool hasValue() const
        {
            bool r = ready();
            assert(r);

            return std::holds_alternative<std::tuple<Ts...>>(value_);
        }

        bool hasException() const
        {
            bool r = ready();
            assert(r);

            return std::holds_alternative<std::exception_ptr>(value_);
        }

        void addCallback(btl::MoveOnlyFunction<void(FutureControl&)> callback)
        {
            if (ready())
            {
                callback(*this);
                return;
            }

            LockType lock(mutex_);

            ready_.load(std::memory_order_acquire);

            // Check if the value was set between calling ready() and
            // locking the mutex..
            if (!std::holds_alternative<std::monostate>(value_))
            {
                lock.unlock();
                callback(*this);
                return;
            }

            if (callback_.has_value())
            {
                callback_ = std::make_optional<MoveOnlyFunction<void(FutureControl&)>>(
                        [oldCb = std::move(callback_),
                            cb=std::move(callback)](FutureControl& control) mutable
                {
                    std::move(*oldCb)(control);
                    std::move(cb)(control);
                });
            }
            else
                callback_ = std::make_optional(std::move(callback));
        }

    private:
        void set(std::variant<std::monostate, std::tuple<Ts...>, std::exception_ptr> value)
        {
            bool r = ready();
            assert(!r);

            LockType lock(mutex_);
            value_ = std::move(value);
            TSAN_ANNOTATE_HAPPENS_BEFORE(&value_);
            TSAN_ANNOTATE_HAPPENS_BEFORE(&value_.getReferenceForTsan());
            ready_.store(true, std::memory_order_release);

            if (callback_.has_value())
            {
                auto callback = std::move(*callback_);
                callback_.reset();

                lock.unlock();

                callback(*this);
            }
        }

    private:
        SpinLock mutex_;
        std::atomic<bool> ready_;
        std::variant<std::monostate, std::tuple<Ts...>, std::exception_ptr> value_;
        std::optional<btl::MoveOnlyFunction<void(FutureControl&)>> callback_;
    };
} // namespace btl::future

