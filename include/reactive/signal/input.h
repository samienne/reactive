#pragma once

#include "inputhandle.h"
#include "signalbase.h"
#include "inputdeferredvalue.h"
#include "sharedsignal.h"
#include "signal.h"

#include <reactive/connection.h>
#include <reactive/observable.h>
#include <reactive/reactivevisibility.h>

#include <btl/dummylock.h>
#include <btl/demangle.h>
#include <btl/hidden.h>

#include <functional>
#include <list>
#include <memory>

namespace reactive::signal
{
    template <typename T, typename TLock = btl::DummyLock>
    class InputSignal;

    template <typename T, typename TLock>
    struct IsSignal<InputSignal<T, TLock>> : std::true_type {};

    template <typename T, typename TLock = btl::DummyLock>
    struct Input;

    template <typename T, typename TLock>
    class InputSignal
    {
    public:
        using Callback = Observable::Callback;

        static_assert(btl::IsClonable<T>::value, "T is not clonable");

        InputSignal(std::shared_ptr<InputDeferredValue<T, TLock>> const& value) :
            deferred_(value),
            tag_(value->getTag(value->lock()))
        {
        }

    public:
        InputSignal(InputSignal const&) = default;
        InputSignal& operator=(InputSignal const&) = default;

    public:
        InputSignal(InputSignal&&) noexcept = default;
        InputSignal& operator=(InputSignal&&) noexcept = default;

        T const& evaluate() const
        {
            if (!value_.valid())
            {
                auto lock = deferred_->lock();
                value_ = btl::just(btl::clone(deferred_->evaluate(lock)));
            }

            return *value_;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            if (frameId_ == frame.getFrameId())
                return btl::none;

            frameId_ = frame.getFrameId();

            auto lock = deferred_->lock();
            return deferred_->updateBegin(lock, frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto lock = deferred_->lock();
            auto r = deferred_->updateEnd(lock, frame);

            changed_ = deferred_->hasChanged(lock, tag_);

            if (changed_ || !value_.valid())
                value_ = btl::just(btl::clone(deferred_->evaluate(lock)));
            tag_ = deferred_->getTag(lock);

            return r;
        }

        bool hasChanged() const
        {
            return changed_;
        };

        Connection observe(Callback const& callback)
        {
            return deferred_->observe(callback);
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("input<" + btl::demangle<T>() + "> changed: "
                    + std::to_string(hasChanged()));
            a.addShared(deferred_.get(), n, deferred_->annotate());
            return a;
        }

        InputSignal clone() const
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

    template <typename T, typename TLock>
    struct Input final
    {
        Input(InputSignal<T, TLock> sig) :
            handle(sig.deferred_),
            signal(share(wrap(std::move(sig))))
        {
        }

        Input(Input const&) = default;
        Input(Input&&) noexcept = default;

        Input& operator=(Input const&) = default;
        Input& operator=(Input&&) noexcept = default;

        InputHandle<T, TLock> handle;
        SharedSignal<InputSignal<T, TLock>, T> signal;
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

