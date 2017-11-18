#pragma once

#include "inputhandle.h"
#include "signalbase.h"
#include "inputdeferredvalue.h"

#include <reactive/sharedsignal.h>
#include <reactive/signal.h>

#include <reactive/connection.h>
#include <reactive/observable.h>

#include <btl/dummylock.h>
#include <btl/demangle.h>
#include <btl/hidden.h>

#include <functional>
#include <list>
#include <memory>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive::signal
{
    template <typename T, typename TLock = btl::DummyLock>
    struct BTL_CLASS_VISIBLE Input;

    template <typename T, typename TLock = btl::DummyLock>
    class BTL_CLASS_VISIBLE InputSignal
    {
    public:
        using Callback = Observable::Callback;

        static_assert(btl::IsClonable<T>::value, "T is not clonable");

        BTL_HIDDEN InputSignal(std::shared_ptr<InputDeferredValue<T, TLock>> const& value) :
            deferred_(value),
            tag_(value->getTag(value->lock()))
        {
        }

    public:
        BTL_HIDDEN InputSignal(InputSignal const&) = default;
        BTL_HIDDEN InputSignal& operator=(InputSignal const&) = default;

    public:
        BTL_HIDDEN InputSignal(InputSignal&&) noexcept = default;
        BTL_HIDDEN InputSignal& operator=(InputSignal&&) noexcept = default;

        BTL_HIDDEN T const& evaluate() const
        {
            if (!value_.valid())
            {
                auto lock = deferred_->lock();
                value_ = btl::just(btl::clone(deferred_->evaluate(lock)));
            }

            return *value_;
        }

        BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
        {
            if (frameId_ == frame.getFrameId())
                return btl::none;

            frameId_ = frame.getFrameId();

            auto lock = deferred_->lock();
            return deferred_->updateBegin(lock, frame);
        }

        BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto lock = deferred_->lock();
            auto r = deferred_->updateEnd(lock, frame);

            changed_ = deferred_->hasChanged(lock, tag_);

            if (changed_ || !value_.valid())
                value_ = btl::just(btl::clone(deferred_->evaluate(lock)));
            tag_ = deferred_->getTag(lock);

            return r;
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return changed_;
        };

        BTL_HIDDEN Connection observe(Callback const& callback)
        {
            return deferred_->observe(callback);
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("input<" + btl::demangle<T>() + "> changed: "
                    + std::to_string(hasChanged()));
            a.addShared(deferred_.get(), n, deferred_->annotate());
            return a;
        }

        BTL_HIDDEN InputSignal clone() const
        {
            return *this;
        }

    private:
        friend struct Input<T, TLock>;

        std::shared_ptr<InputDeferredValue<T, TLock>> deferred_;
        uint64_t frameId_ = 0;
        uint32_t tag_ = 0;
        mutable btl::option<T> value_;
        bool changed_ = false;
    };

    static_assert(IsSignal<InputSignal<int, btl::DummyLock>>::value,
        "InputSignal is not a signal");

    template <typename T, typename TLock>
    struct BTL_CLASS_VISIBLE Input final
    {
        BTL_HIDDEN Input(InputSignal<T, TLock> sig) :
            handle(sig.deferred_),
            signal(share(wrap(std::move(sig))))
        {
        }

        BTL_HIDDEN Input(Input const&) = default;
        BTL_HIDDEN Input(Input&&) noexcept = default;

        BTL_HIDDEN Input& operator=(Input const&) = default;
        BTL_HIDDEN Input& operator=(Input&&) noexcept = default;

        InputHandle<T, TLock> handle;
        SharedSignal<T, InputSignal<T, TLock>> signal;
    };

    template <typename T, typename TLock = btl::DummyLock>
    constexpr Input<T, TLock> input(T initial)
    {
        auto sig = InputSignal<T, TLock>(
                    std::make_shared<InputDeferredValue<T, TLock>>(
                        std::move(initial)
                    ));

        return Input<T, TLock>(std::move(sig));
    }

} // reactive::signal

BTL_VISIBILITY_POP

