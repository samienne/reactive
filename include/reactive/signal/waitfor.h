#pragma once

#include "input.h"

#include <btl/future/future.h>
#include <btl/spinlock.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename T>
        auto waitFor(T initial, btl::future::Future<T> f)
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

BTL_VISIBILITY_POP

