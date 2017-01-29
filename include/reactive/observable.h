#pragma once

#include "connection.h"

#include <btl/spinlock.h>
#include <btl/observable.h>

#include <functional>
#include <list>
#include <memory>
#include <atomic>
#include <mutex>

namespace reactive
{
    class Observable : public btl::detail::observable_control_base
    {
    public:
        using Callback = std::function<void()>;

        Observable()
        {
        }

        virtual ~Observable()
        {
        }

        void notify() const
        {
            decltype(callbacks_) cbs;

            {
                std::lock_guard<btl::SpinLock> lock(mutex_);
                if (!enabled_)
                    return;

                cbs = callbacks_;
            }

            for (auto& cb : cbs)
                cb.second();
        }

        btl::connection observe(Callback callback)
        {
            std::lock_guard<btl::SpinLock> lock(mutex_);
            auto i = nextIndex_++;
            callbacks_.push_back(std::make_pair(i, callback));
            return btl::connection(i, shared_from_this());
        }

        void enable(bool b)
        {
            enabled_ = b;
        }

    private:
        void disconnect(size_t index) override
        {
            std::lock_guard<btl::SpinLock> lock(mutex_);
            for (auto i = callbacks_.begin(); i != callbacks_.end(); ++i)
            {
                if (i->first == index)
                {
                    callbacks_.erase(i);
                    return;
                }
            }
        }

    private:
        std::vector<std::pair<size_t, Callback>> callbacks_;
        mutable btl::SpinLock mutex_;
        size_t nextIndex_ = 1;
        bool enabled_ = true;
    };
}

