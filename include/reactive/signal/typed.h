#pragma once

#include "signalbase.h"

#include <iostream>

namespace reactive::signal
{
    template <typename TSignal, typename... Ts>
    class Signal;

    template <typename TSignal, typename... Ts>
    struct IsSignal<Signal<TSignal, Ts...>> : std::true_type {};

    template <typename TDeferred, typename... Ts>
    class Share;

    template <typename TSignal, typename... Ts>
    class Typed;

    template <typename TDeferred, typename T>
    struct IsSignal<Share<TDeferred, T>> : std::true_type {};

    template <typename TSignal, typename T>
    struct IsSignal<Typed<TSignal, T>> : std::true_type {};

    template <typename TDeferred, typename... Ts>
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

    template <typename TSignal, typename... Ts>
    class Typed final : public SignalBase<Ts...>
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

        typename detail::SignalBaseResult<Ts...>::type evaluate() const override final
        {
            return sig_.evaluate();
        }

        bool hasChanged() const override final
        {
            return sig_.hasChanged();
        }

        std::optional<signal_time_t> updateBegin(FrameInfo const& frame)
            override final
        {
            if (frameId_ == frame.getFrameId())
                return std::nullopt;

            frameId_ = frame.getFrameId();
            return sig_.updateBegin(frame);
        }

        std::optional<signal_time_t> updateEnd(FrameInfo const& frame)
            override final
        {
            std::optional<signal_time_t> r = std::nullopt;
            if(frameId_ != frame.getFrameId())
                r = sig_.updateBegin(frame);

            if (frameId2_ == frame.getFrameId())
                return std::nullopt;

            frameId2_ = frame.getFrameId();
            auto r2 = sig_.updateEnd(frame);

            if (!r2.has_value())
                return r;
            else if(!r.has_value())
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

    template <typename... Ts, typename U, typename... Vs>
    auto typed(Signal<U, Vs...> sig)
    {
        return Share<SignalBase<Ts...>, Ts...>(
                    std::make_shared<Typed<Signal<U, Vs...>, Ts...>>(std::move(sig))
                    );
    }

    template <typename... Ts>
    auto typed(Signal<void, Ts...> sig)
    {
        return sig.storage();
    }
} // reactive::signal

