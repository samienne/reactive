#pragma once

#include "signalbase.h"

#include <iostream>

namespace reactive
{
    template <typename TSignal, typename T>
    class Signal;

    template <typename TSignal, typename T>
    struct IsSignal<Signal<TSignal, T>> : std::true_type {};

    namespace signal
    {
        template <typename TDeferred, typename T>
        class Share;

        template <typename TSignal, typename T>
        class Typed;
    }

    template <typename TDeferred, typename T>
    struct IsSignal<signal::Share<TDeferred, T>> : std::true_type {};

    template <typename TSignal, typename T>
    struct IsSignal<signal::Typed<TSignal, T>> : std::true_type {};
} // reactive

namespace reactive::signal
{
    template <typename TDeferred, typename T>
    class Share
    {
    public:
        Share(std::shared_ptr<TDeferred> deferred) :
            control_(std::move(deferred))
        {
        }

        Share(Share const&) = default;
        Share& operator=(Share const&) = default;
        Share(Share&&) noexcept = default;
        Share& operator=(Share&&) noexcept = default;

        auto evaluate() const -> decltype(auto)
        {
            return control_->evaluate();
        }

        bool hasChanged() const
        {
            return control_->hasChanged();
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return control_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            return control_->updateEnd(frame);
        }

        template <typename TFunc>
        Connection observe(TFunc&& callback)
        {
            return control_->observe(std::forward<TFunc>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            return a;
        }

        Share clone() const
        {
            return *this;
        }

        using ValueType = decltype(std::declval<TDeferred>().evaluate());

        std::weak_ptr<SignalBase<ValueType>> weak() const
        {
            return control_.ptr();
        }

        std::shared_ptr<SignalBase<ValueType>> ptr() const &
        {
            return control_.ptr();
        }

        std::shared_ptr<SignalBase<ValueType>> ptr() &&
        {
            return std::move(control_.ptr());
        }

    private:
        btl::shared<TDeferred> control_;
    };

    template <typename TSignal, typename T>
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

        Typed(TSignal&& sig) :
            sig_(std::move(sig))
        {
        }

        ~Typed()
        {
        }

        T evaluate() const override final
        {
            return sig_.evaluate();
        }

        bool hasChanged() const override final
        {
            return sig_.hasChanged();
        }

        btl::option<signal_time_t> updateBegin(signal::FrameInfo const& frame)
            override final
        {
            if (frameId_ == frame.getFrameId())
                return btl::none;

            frameId_ = frame.getFrameId();
            return sig_.updateBegin(frame);
        }

        btl::option<signal_time_t> updateEnd(signal::FrameInfo const& frame)
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

        btl::connection observe(
                std::function<void()> const& callback) override final
        {
            return sig_.observe(callback);
        }

        Annotation annotate() const override final
        {
            return Annotation();
        }

        bool isCached() const override
        {
            return std::is_reference<SignalType<TSignal>>::value;
        }

    private:
        TSignal sig_;
        uint64_t frameId_ = 0;
        uint64_t frameId2_ = 0;
    };

    template <typename V, typename T, typename U>
    auto typed(Signal<U, T> sig)
    {
        return std::make_shared<signal::Typed<U, V>>(std::move(sig).signal());
    }

    template <typename U, typename T>
    auto typed(Signal<void, T> sig)
    {
        return typed<U>(wrap(signal::Share<signal::Typed<Signal<void, T>, T>, T>(
                    std::make_shared<signal::Typed<Signal<void, T>, T>>(std::move(sig)))
                ));
    }

    template <typename T>
    auto typed(Signal<void, T> sig)
    {
        return sig.getDeferredSignalBase();
    }
} // reactive::signal

