#pragma once

#include "spinlock.h"

#include <memory>
#include <mutex>
#include <vector>

namespace btl
{
    namespace detail
    {
        struct observable_control_base : public
            std::enable_shared_from_this<observable_control_base>
        {
            virtual ~observable_control_base()
            {
            }

            virtual void disconnect(size_t index) = 0;
        };

        template <typename... TArgs>
        struct observable_control : public observable_control_base
        {
            void disconnect(size_t index) override
            {
                std::unique_lock<SpinLock> lock(mutex);
                for (auto i = callbacks.begin(); i != callbacks.end(); ++i)
                {
                    if (i->first == index)
                    {
                        callbacks.erase(i);
                        return;
                    }
                }
            }

            std::vector<std::pair<size_t, std::function<void(TArgs...)>>>
                callbacks;
            mutable btl::SpinLock mutex;
            size_t nextIndex = 1;
        };
    } // detail
} // btl

