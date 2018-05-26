#pragma once

#include "signalbase.h"

#include <btl/hidden.h>

#include <iostream>

namespace reactive
{
    template <typename T, typename TSignal = void>
    class Signal;

    template <typename T, typename TSignal>
    struct IsSignal<Signal<T, TSignal>> : std::true_type {};

    namespace signal
    {
        template <typename T, typename TDeferred>
        class Share;

        template <typename T, typename TSignal>
        class Typed;
    }

    template <typename T, typename TDeferred>
    struct IsSignal<signal::Share<T, TDeferred>> : std::true_type {};

    template <typename T, typename TSignal>
    struct IsSignal<signal::Typed<T, TSignal>> : std::true_type {};
} // reactive

namespace reactive::signal
{
    template <typename T, typename TDeferred>
    class Share
    {
    public:
        BTL_HIDDEN Share(std::shared_ptr<TDeferred> deferred) :
            control_(std::move(deferred))
        {
        }

        BTL_HIDDEN Share(Share const&) = default;
        BTL_HIDDEN Share& operator=(Share const&) = default;
        BTL_HIDDEN Share(Share&&) noexcept = default;
        BTL_HIDDEN Share& operator=(Share&&) noexcept = default;

        BTL_HIDDEN auto evaluate() const -> decltype(auto)
        {
            return control_->evaluate();
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return control_->hasChanged();
        }

        BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
        {
            return control_->updateBegin(frame);
        }

        BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
        {
            return control_->updateEnd(frame);
        }

        template <typename TFunc>
        BTL_HIDDEN Connection observe(TFunc&& callback)
        {
            return control_->observe(std::forward<TFunc>(callback));
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            return a;
        }

        BTL_HIDDEN Share clone() const
        {
            return *this;
        }

        using ValueType = decltype(std::declval<TDeferred>().evaluate());

        BTL_HIDDEN std::weak_ptr<SignalBase<ValueType>> weak() const
        {
            return control_.ptr();
        }

        BTL_HIDDEN std::shared_ptr<SignalBase<ValueType>> ptr() const &
        {
            return control_.ptr();
        }

        BTL_HIDDEN std::shared_ptr<SignalBase<ValueType>> ptr() &&
        {
            return std::move(control_.ptr());
        }

    private:
        btl::shared<TDeferred> control_;
    };

    template <typename T, typename TSignal>
    class Typed final : public signal::SignalBase<T>
    {
    public:
        using Lock = std::lock_guard<btl::SpinLock>;

        /*
        static_assert(
                !std::is_reference<T>::value
                || ( std::is_reference<T>::value
                    && std::is_reference<SignalType<TSignal>>::value),
                "");
                */

        BTL_HIDDEN Typed(TSignal&& sig) :
            sig_(std::move(sig))
        {
        }

        BTL_HIDDEN ~Typed()
        {
        }

        BTL_HIDDEN T evaluate() const override final
        {
            return sig_.evaluate();
        }

        BTL_HIDDEN bool hasChanged() const override final
        {
            return sig_.hasChanged();
        }

        BTL_HIDDEN btl::option<signal_time_t> updateBegin(signal::FrameInfo const& frame)
            override final
        {
            if (frameId_ == frame.getFrameId())
                return btl::none;

            frameId_ = frame.getFrameId();
            return sig_.updateBegin(frame);
        }

        BTL_HIDDEN btl::option<signal_time_t> updateEnd(signal::FrameInfo const& frame)
            override final
        {
            btl::option<signal_time_t> r = btl::none;
            if(frameId_ != frame.getFrameId())
                r = sig_.updateBegin(frame);

            if (frameId2_ == frame.getFrameId())
                return btl::none;

            frameId2_ = frame.getFrameId();
            auto r2 = sig_.updateEnd(frame);

            if (!r2.valid())
                return r;
            else if(!r.valid())
                return r2;
            else
                return std::min(r2, r);
        }


        BTL_HIDDEN btl::connection observe(
                std::function<void()> const& callback) override final
        {
            return sig_.observe(callback);
        }

        BTL_HIDDEN Annotation annotate() const override final
        {
            return Annotation();
        }

        BTL_HIDDEN bool isCached() const override
        {
            return std::is_reference<SignalType<TSignal>>::value;
        }

    private:
        TSignal sig_;
        uint64_t frameId_ = 0;
        uint64_t frameId2_ = 0;
    };

    template <typename V, typename T, typename U>
    auto typed(Signal<T, U> sig)
    {
        return std::make_shared<signal::Typed<V, U>>(std::move(sig).signal());
    }

    template <typename U, typename T>
    auto typed(Signal<T, void> sig)
    {
        return typed<U>(wrap(signal::Share<T, signal::Typed<T, Signal<T>>>(
                    std::make_shared<signal::Typed<T, Signal<T>>>(std::move(sig)))
                ));
    }

    template <typename T>
    auto typed(Signal<T, void> sig)
    {
        return sig.getDeferredSignalBase();
    }
} // reactive::signal

