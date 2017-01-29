#pragma once

#include "input.h"

#include <btl/future/future.h>

#include <btl/spinlock.h>

namespace reactive
{
    namespace signal
    {
        template <typename T>
        signal::InputSignal<T, btl::SpinLock> waitFor(
                T initial, btl::future::Future<T> f)
        {
            auto i = signal::input<T, btl::SpinLock>(std::move(initial));

            std::move(f)
                .fmap([h=std::move(i.handle)](T value) mutable
                {
                    h.set(std::move(value));
                    return true;
                })
                .detach();

            return std::move(i.signal);
        }
    } // signal
} // namespace

