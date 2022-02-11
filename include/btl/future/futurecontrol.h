#pragma once

#include "traits.h"
#include "futurebase.h"

#include "../threadpool.h"

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace btl
{
    namespace future
    {
        namespace detail
        {
            struct DoMap;

            // Use 2-phase lookup to "break" circular dependency
            template <typename T>
            struct MyDoMap
            {
                using type = future::detail::DoMap;
            };
        }

        template <typename T>
        struct FutureControl;

        template <typename T>
        auto get(T&& future)
            -> decltype(future->get())
        {
            return std::forward<T>(future)->get();
        }

        template <typename T>
        auto isReady(T&& future)
            -> decltype(future->isReady())
        {
            return std::forward<T>(future)->isReady();
        }

        template <typename T>
        struct FutureControl final : public FutureBase<T>
        {
            FutureControl()
            {
                ready.store(false, std::memory_order_release);
            }

            T get() override
            {
                if (!isReady())
                {
                    std::mutex mutex;
                    std::condition_variable condition;

                    std::unique_lock<std::mutex> lock(mutex);
                    auto selfPtr = this->shared_from_this();
                    /*auto f = typename detail::MyDoMap<T>::type()(
                            [&](T const&)
                            {
                                std::unique_lock<std::mutex> lock(mutex);
                                condition.notify_all();
                                return true;
                            }, std::move(selfPtr));*/

                    ThreadPool::getInstance().flush();
                    condition.wait(lock);
                }

                assert(value.has_value());

                return *std::move(value);
            }

            bool isReady() const override
            {
                return ready.load(std::memory_order_acquire);
            }

            std::optional<T> value;
            std::atomic<bool> ready;
        };

        static_assert(IsFuture<FutureControl<int>>::value, "");

    } // future
} // btl

