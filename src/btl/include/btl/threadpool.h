#pragma once

#include "moveonlyfunction.h"

#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <assert.h>

namespace btl
{
    struct TimedTask
    {
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimePoint timePoint;
        MoveOnlyFunction<void()> function;

        bool operator<(TimedTask const& rhs) const noexcept
        {
            // Order is reversed
            return timePoint > rhs.timePoint;
        }
    };

    class TimedQueue
    {
    public:
        using Clock = TimedTask::Clock;
        using TimePoint = TimedTask::TimePoint;

        template <typename TFunc>
        inline void push(TimePoint timePoint, TFunc&& func)
        {
            tasks_.push_back(TimedTask{
                    timePoint,
                    std::forward<TFunc>(func)
                    });
        }

        inline MoveOnlyFunction<void()> pop() noexcept
        {
            auto task = std::move(tasks_.front());
            std::pop_heap(tasks_.begin(), tasks_.end());
            tasks_.pop_back();
            return std::move(task.function);
        }

        inline bool hasTaskReady() const noexcept
        {
            if (tasks_.empty())
                return false;

            auto time = Clock::now();
            return tasks_.front().timePoint <= time;
        }

        inline std::optional<TimePoint> getNextEventTime() const noexcept
        {
            if (tasks_.empty())
                return std::nullopt;

            return tasks_.front().timePoint;
        }

        inline bool empty() const noexcept
        {
            return tasks_.empty();
        }

    private:
        std::vector<TimedTask> tasks_;
    };

    class TaskQueue
    {
        using LockType = std::unique_lock<std::mutex>;

    public:
        inline void done()
        {
            {
                LockType lock(mutex_);
                done_ = true;
            }

            condition_.notify_all();
        }

        template <typename TFunc>
        inline void push(TFunc&& func)
        {
            {
                LockType lock(mutex_);
                tasks_.emplace_back(std::forward<TFunc>(func));
            }
            condition_.notify_one();
        }

        template <typename TFunc>
        inline void pushTimed(TimedQueue::TimePoint timePoint, TFunc&& func)
        {
            {
                LockType lock(mutex_);
                timedQueue_.push(timePoint, std::forward<TFunc>(func));
            }
            condition_.notify_one();
        }

        template <typename TFunc>
        inline bool tryPush(TFunc&& func)
        {
            {
                LockType lock(mutex_, std::try_to_lock);
                if (!lock)
                    return false;
                tasks_.emplace_back(std::forward<TFunc>(func));
            }

            condition_.notify_one();

            return true;
        }

        template <typename TFunc>
        inline bool tryPushTimed(TimedQueue::TimePoint timePoint, TFunc&& func)
        {
            {
                LockType lock(mutex_, std::try_to_lock);
                if (!lock)
                    return false;
                timedQueue_.push(timePoint, std::forward<TFunc>(func));
            }

            condition_.notify_one();

            return true;
        }

        inline std::optional<MoveOnlyFunction<void()>> pop()
        {
            LockType lock(mutex_);
            while (tasks_.empty() && !timedQueue_.hasTaskReady() && !done_)
            {
                auto time = timedQueue_.getNextEventTime();
                if (time.has_value())
                    condition_.wait_until(lock, *time);
                else
                    condition_.wait(lock);
            }

            if (timedQueue_.hasTaskReady())
                return timedQueue_.pop();

            if (tasks_.empty())
                return std::nullopt;

            auto task = std::move(tasks_.front());
            tasks_.pop_front();

            return task;
        }

        inline std::optional<MoveOnlyFunction<void()>> tryPop()
        {
            LockType lock(mutex_, std::try_to_lock);
            bool hasTimedTask = !lock || timedQueue_.hasTaskReady();
            if (!lock || (tasks_.empty() && !hasTimedTask))
                return std::nullopt;

            if (hasTimedTask)
                return timedQueue_.pop();

            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            return task;
        }

    private:
        std::deque<MoveOnlyFunction<void()>> tasks_;
        TimedQueue timedQueue_;
        std::mutex mutex_;
        std::condition_variable condition_;
        bool done_ = false;
    };

    class ThreadPool
    {
    public:
        using Duration = TimedQueue::Clock::duration;

        static ThreadPool& getInstance()
        {
            static ThreadPool pool;
            return pool;
        }

        inline ThreadPool()
        {
            std::cout << "Starting thread pool with " << count_
                << " threads." << std::endl;
            for (unsigned int n = 0; n != count_; ++n)
            {
                threads_.emplace_back([&, n]
                    {
                        run(n);
                    });
            }
        }

        inline ~ThreadPool()
        {
            std::cout << "~ThreadPool()" << std::endl;
            for (auto&& queue : queues_)
                queue.done();
            for (auto&& thread : threads_)
                thread.join();
        }

        void async(MoveOnlyFunction<void()> func)
        {
            auto i = index_.fetch_add(1, std::memory_order_relaxed);
            for (unsigned n = 0; n != count_; ++n)
            {
                if (queues_[(i+n) % count_].tryPush(std::move(func)))
                    return;
            }

            queues_[i % count_].push(std::move(func));
        }

        template <typename TFunc, typename T, typename... Ts>
        void async(TFunc&& func, T&& t, Ts&&... ts)
        {
            async(std::bind(
                        std::forward<TFunc>(func),
                        std::forward<T>(t),
                        std::forward<Ts>(ts)...
                        )
                 );
        }

        void delayed(Duration delay, MoveOnlyFunction<void()> func)
        {
            auto time = TimedQueue::Clock::now() + delay;
            auto i = index_.fetch_add(1, std::memory_order_relaxed);

            for (unsigned n = 0; n != count_; ++n)
            {
                if (queues_[(i+n) % count_].tryPushTimed(time, std::move(func)))
                    return;
            }

            queues_[i % count_].pushTimed(time, std::move(func));
        }

        template <typename TFunc, typename T, typename... Ts>
        void delayed(Duration delay, TFunc&& func, T&& t, Ts&&... ts)
        {
            delayed(delay, std::bind(
                        std::forward<TFunc>(func),
                        std::forward<T>(t),
                        std::forward<Ts>(ts)...
                        )
                 );
        }

    private:
        void run(unsigned i)
        {
            while (true)
            {
                std::optional<MoveOnlyFunction<void()>> f;

                for (unsigned n = 0; n != count_ * 32u; ++n)
                {
                    f = queues_[(n+i) % count_].tryPop();
                    if (f.has_value())
                        break;
                }

                if (!f.has_value())
                    f = queues_[i].pop();

                if (!f.has_value())
                    break;

                std::move(*f)();
            }
        }

    private:
        size_t count_{std::thread::hardware_concurrency()};
        std::vector<std::thread> threads_;
        std::vector<TaskQueue> queues_{count_};
        std::atomic<unsigned int> index_{0};
    };
} // btl

