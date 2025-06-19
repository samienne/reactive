#pragma once

#include "observablecontrolbase.h"
#include "connection.h"
#include "spinlock.h"
#include "shared.h"

#include <functional>
#include <vector>
#include <mutex>

namespace btl
{
    template <typename>
    class observable;

    template <typename... TArgs>
    class observable<void(TArgs...)>
    {
    public:
        using callback_t = std::function<void(TArgs...)>;
        using lock_t = std::unique_lock<SpinLock>;

        inline observable() :
            d_(std::make_shared<detail::observable_control<TArgs...>>())
        {
        }

        observable(observable const&) = default;
        observable(observable&&) noexcept = default;
        observable& operator=(observable const&) = default;
        observable& operator=(observable&&) noexcept = default;

        inline connection observe(callback_t cb)
        {
            lock_t lock(d_->mutex);
            auto i = d_->nextIndex++;
            d_->callbacks.push_back(std::make_pair(i , std::move(cb)));
            std::weak_ptr<detail::observable_control_base> weak = d_.ptr();

            return connection::on_disconnect(
                    [i, weak=std::move(weak)]() mutable
                    {
                        if (auto p = weak.lock())
                        {
                            p->disconnect(i);
                            weak.reset();
                        }
                    });
        }

        inline void operator()(TArgs... args) const
        {
            lock_t lock(d_->mutex);
            std::vector<callback_t> cbs;
            cbs.reserve(d_->callbacks.size());
            for (auto const& cb : d_->callbacks)
                cbs.push_back(cb.second);
            lock.unlock();

            for (auto const& cb : cbs)
                cb(args...);
        }

    private:
        btl::shared<detail::observable_control<TArgs...>> d_;
    };
}

