#pragma once

#include "inputdeferredvalue.h"
#include "signalbase.h"

#include <reactive/signaltraits.h>

#include <btl/dummylock.h>

#include <memory>
#include <utility>
#include <iostream>

namespace reactive
{
    namespace signal
    {
        template <typename T, typename TLock = btl::DummyLock>
        class InputHandle
        {
        public:
            InputHandle(std::weak_ptr<InputDeferredValue<T, TLock>> value) :
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

            void set(T value)
            {
                if (auto p = deferred_.lock())
                    p->set(p->lock(), std::move(value));
            }

            void set(signal2::Signal<T, Weak<T>> const& sig)
            {
                if (auto p = deferred_.lock())
                    p->set(p->lock(), sig);
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
    }
}

