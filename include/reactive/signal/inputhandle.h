#pragma once

#include "inputdeferredvalue.h"
#include "signalbase.h"

#include <reactive/signaltraits.h>
#include "signal.h"

#include <btl/dummylock.h>
#include <btl/hidden.h>

#include <memory>
#include <utility>
#include <iostream>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive::signal
{
    template <typename T, typename TLock = btl::DummyLock>
    class BTL_CLASS_VISIBLE InputHandle
    {
    public:
        BTL_HIDDEN InputHandle(std::weak_ptr<InputDeferredValue<T, TLock>> value) :
            deferred_(std::move(value))
        {
        }

        /*
        void set(T const& value)
        {
            if (auto p = deferred_.lock())
                p->set(p->lock(), value);
        }
        */

        BTL_HIDDEN void set(T value)
        {
            if (auto p = deferred_.lock())
                p->set(p->lock(), std::move(value));
        }

        BTL_HIDDEN void set(Signal<T, Weak<T>> sig)
        {
            if (auto p = deferred_.lock())
                p->set(p->lock(), std::move(sig));
        }

        BTL_HIDDEN bool operator==(InputHandle const& rhs) const
        {
            return deferred_.lock() == rhs.deferred_.lock();
        }

        BTL_HIDDEN bool operator!=(InputHandle const& rhs) const
        {
            return deferred_.lock() == rhs.deferred_.lock();
        }

    private:
        std::weak_ptr<InputDeferredValue<T, TLock>> deferred_;
    };
} // namespace reactive::signal

BTL_VISIBILITY_POP

