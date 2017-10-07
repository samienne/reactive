#pragma once

#include "constant.h"
#include "weak.h"

#include "reactive/observable.h"

#include <btl/spinlock.h>

#include <mutex>

namespace reactive
{
    namespace signal
    {

    template <typename T, typename TLock>
    class InputDeferredValue : public Observable
    {
    public:
        using Lock = std::unique_lock<TLock>;

        InputDeferredValue(T&& initial) :
            value_(std::move(initial))
        {
        }

        InputDeferredValue(InputDeferredValue const&) = delete;
        InputDeferredValue operator=(InputDeferredValue const) = delete;

        T evaluate(Lock const&) const
        {
            return btl::clone(value_);
        }

        UpdateResult updateBegin(Lock const&, FrameInfo const& frame)
        {
            if (frameId_ == frame.getFrameId())
                return btl::none;

            frameId_ = frame.getFrameId();

            auto r = signal_.updateBegin(frame);

            return r;
        }

        UpdateResult updateEnd(Lock const&, FrameInfo const& frame)
        {
            assert(frameId_ == frame.getFrameId());

            if (frameId2_ == frame.getFrameId())
                return btl::none;

            frameId2_ = frame.getFrameId();

            auto r = signal_.updateEnd(frame);

            bool changed = signal_.hasChanged();
            if (changed || signalChanged_)
            {
                signalChanged_ = false;
                if (signal_.isValid())
                    value_ = signal_.evaluate();
                ++tag_;
            }

            return r;
        }

        bool hasChanged(Lock const&, uint32_t tag) const
        {
            return tag != tag_;
        }

        uint32_t getTag(Lock const&) const
        {
            return tag_;
        }

        void set(Lock const&, signal2::Signal<T, Weak<T>> sig)
        {
            if (signal_ == sig.signal())
                //return;

            //value_ = sig.evaluate();
            signalChanged_ = true;
            signal_ = std::move(sig).signal();
            ++tag_;

            connection_ = Connection();
            notify();
            doObserve();
        }

        /*
        void set(Lock const&, T const& value)
        {
            value_ = value;
            ++tag_;
            connection_ = Connection();
            notify();
            doObserve();
        }
        */

        void set(Lock const&, T&& value)
        {
            value_ = std::move(value);
            ++tag_;
            connection_ = Connection();
            notify();
            doObserve();
        }

        Lock lock()
        {
            return Lock(spin_);
            //return Lock();
        }

        void doObserve()
        {
            connection_ = signal_.observe([this]()
                    {
                        this->enable(false);
                        this->notify();
                        this->enable(true);
                    });
        }

        Annotation annotate() const
        {
            if (annotating_)
                return Annotation();

            annotating_ = true;
            Annotation a;
            auto&& n = a.addNode("InputDeferredValue");
            a.addTree(n, signal_.annotate());
            annotating_ = false;
            return a;
        }

    private:
        uint64_t frameId_ = 0;
        uint64_t frameId2_ = 0;
        mutable TLock spin_;
        uint32_t tag_ = 0;
        Weak<T> signal_;
        Connection connection_;
        T value_;
        bool signalChanged_ = false;
        mutable bool annotating_ = false;
    };

    }
}

