#pragma once

#include <btl/moveonlyfunction.h>
#include <btl/option.h>
#include <btl/spinlock.h>

#include <btl/tsan.h>

#include <condition_variable>
#include <memory>
#include <mutex>

namespace btl
{
    namespace future
    {
        class FutureBase : public std::enable_shared_from_this<FutureBase>
        {
        public:
            virtual ~FutureBase() = default;
        };

        template <typename T>
        class FutureControl : public FutureBase
        {
            using LockType = std::unique_lock<SpinLock>;

        public:
            FutureControl() :
                ready_(false)
            {
            }

            void set(T value)
            {
                assert(!ready());

                LockType lock(mutex_);
                value_ = btl::just(std::forward<T>(value));
                TSAN_ANNOTATE_HAPPENS_BEFORE(&value_);
                TSAN_ANNOTATE_HAPPENS_BEFORE(&value_.getReferenceForTsan());
                ready_.store(true, std::memory_order_release);

                if (callback_.valid())
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
                if (value_.valid())
                    return;

                //assert(!callback_);

                std::mutex mutex;
                std::condition_variable condition;

                auto oldCallback = std::move(callback_);
                callback_ = btl::just<MoveOnlyFunction<void()>>(
                        [&condition, &mutex]() mutable
                {
                    std::unique_lock<std::mutex> lock2(mutex);
                    condition.notify_one();
                });

                std::unique_lock<std::mutex> lock2(mutex);

                lock.unlock();
                condition.wait(lock2);
                if (oldCallback.valid())
                    (*oldCallback)();
            }

            T get()
            {
                waitForResult();

                // No need for lock as the value will be set only once
                // and we know that it has been set already. Also we are
                // the only consumer for the value.
                return std::move(*value_);
            }

            std::decay_t<T> const& getRef()
            {
                waitForResult();

                assert(value_.valid());

                // No need for lock as the value will be set only once
                // and we know that it has been set already.
                return *value_;
            }

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
                if (value_.valid())
                {
                    lock.unlock();
                    callback();
                    return;
                }

                if (callback_.valid())
                {
                    callback_ = btl::just<MoveOnlyFunction<void()>>(
                            [oldCb = std::move(callback_),
                                cb=std::move(callback)]() mutable
                    {
                        std::move(*oldCb)();
                        std::move(cb)();
                    });
                }
                else
                    callback_ = btl::just(std::move(callback));
            }

        private:
            SpinLock mutex_;
            std::atomic<bool> ready_;
            btl::option<T> value_;
            btl::option<btl::MoveOnlyFunction<void()>> callback_;
        };

    } // future
} // btl

