#pragma once

#include "taskbase.h"
#include "taskstatus.h"
#include "spinlock.h"

#include <future>
#include <deque>
#include <functional>
#include <thread>
#include <iostream>
#include <memory>
#include <atomic>
#include <vector>

namespace btl
{
#if 0
    class TaskRunner
    {
        using lock_t = std::unique_lock<std::mutex>;

    public:
        inline TaskRunner()
        {
            thread_ = std::thread([this] { TaskRunner::loop(); });
        }

        inline ~TaskRunner()
        {
            running_ = false;

            waitCondition_.notify_all();
            thread_.join();
        }

        inline void addTask(std::unique_ptr<TaskBase> task)
        {
            {
                lock_t lock(waitMutex_);
                tasks_.push_back(std::move(task));
                waitCondition_.notify_all();
            }
        }

        inline void addTasks(std::vector<std::unique_ptr<TaskBase>> tasks)
        {
            {
                lock_t lock(waitMutex_);
                for (auto&& task : tasks)
                    tasks_.push_back(std::move(task));

                waitCondition_.notify_all();
            }
        }

        /*inline std::unique_ptr<TaskBase> tryPop()
        {
            {
                std::unique_lock<std::mutex> lock(waitMutex_);

                if (!tasks_.empty())
                {
                    auto task = std::move(tasks_.front());
                    tasks_.pop_front();
                    lock.unlock();
                    return std::move(task);
                }
            }

            return nullptr;
        }*/

        inline std::unique_ptr<TaskBase> pop()
        {
            std::unique_lock<std::mutex> lock(waitMutex_);

            if (tasks_.empty())
                waitCondition_.wait(lock);

            if (tasks_.empty())
                return nullptr;

            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            lock.unlock();
            return task;
        }

    private:
        void notifyTasks()
        {
            waitCondition_.notify_all();
        }

        void loop()
        {
            running_ = true;
            while (running_)
            {
                auto task = pop();
                if (task)
                {
                    auto status = task->run();
                    if (status != TaskStatus::finished)
                        addTask(std::move(task));
                }
            }
        }

    private:
        std::thread thread_;
        std::deque<std::unique_ptr<TaskBase>> tasks_;
        //SpinLock lock_;
        std::mutex waitMutex_;
        std::condition_variable waitCondition_;
        std::atomic<bool> running_;
    };
#endif
} // btl

