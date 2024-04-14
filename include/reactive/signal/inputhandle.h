#pragma once

#include "inputdeferredvalue.h"
#include "signalbase.h"
#include "signaltraits.h"
#include "signal.h"

#include <btl/dummylock.h>
#include <btl/hidden.h>

#include <memory>
#include <utility>

namespace reactive::signal
{
    template <typename T, typename TLock = btl::DummyLock>
    class InputHandle
    {
    public:
        InputHandle(std::weak_ptr<InputDeferredValue<T, TLock>> value) :
            deferred_(std::move(value))
        {
        }

        InputHandle(InputHandle&&) = default;
        InputHandle(InputHandle const&) = default;

        InputHandle& operator=(InputHandle&&) noexcept = default;
        InputHandle& operator=(InputHandle const&) = default;

        /*
        void set(T const& value)
        {
            if (auto p = deferred_.lock())
                p->set(p->lock(), value);
        }
        */

        void set(T value)
        {
            if (auto p = deferred_.lock())
                p->set(p->lock(), std::move(value));
        }

        void set(Signal<Weak<T>, T> sig)
        {
            if (auto p = deferred_.lock())
                p->set(p->lock(), std::move(sig));
        }

        bool operator==(InputHandle const& rhs) const
        {
            return deferred_.lock() == rhs.deferred_.lock();
        }

        bool operator!=(InputHandle const& rhs) const
        {
            return deferred_.lock() == rhs.deferred_.lock();
        }

    private:
        std::weak_ptr<InputDeferredValue<T, TLock>> deferred_;
    };
} // namespace reactive::signal

