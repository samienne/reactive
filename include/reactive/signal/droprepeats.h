#pragma once

#include "constant.h"
#include "signal.h"
#include "signaltraits.h"

namespace reactive::signal
{
    template <typename TSignal>
    class DropRepeats;

    template <typename TSignal>
    struct IsSignal<DropRepeats<TSignal>> : std::true_type {};

    template <typename TSignal>
    class DropRepeats
    {
    public:
        DropRepeats(TSignal signal) :
            signal_(std::move(signal))
        {
        }

        DropRepeats(DropRepeats&&) noexcept = default;
        DropRepeats& operator=(DropRepeats&&) noexcept = default;

        signal_value_t<TSignal> const& evaluate() const
        {
            if (!value_.has_value())
                value_ = std::make_optional(btl::clone(signal_->evaluate()));

            return *value_;
        }

        bool hasChanged() const
        {
            return changed_;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return signal_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = signal_->updateEnd(frame);

            auto value = btl::clone(signal_->evaluate());

            if (!value_.has_value())
            {
                value_ = std::make_optional(std::move(value));
                changed_ = signal_->hasChanged();
                return r;
            }

            changed_ = !(value == *value_);
            value_ = std::make_optional(std::move(value));

            return r;
        }

        template <typename TCallback>
        Connection observe(TCallback&& callback)
        {
            return signal_->observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("dropRepeats() + changed: "
                    + std::to_string(hasChanged()));
            a.addTree(n, signal_->annotate());
            return a;
        }

        DropRepeats clone() const
        {
            return *this;
        }

    private:
        DropRepeats(DropRepeats const&) = default;
        DropRepeats& operator=(DropRepeats const&) = default;

    private:
        btl::CloneOnCopy<std::decay_t<TSignal>> signal_;
        mutable std::optional<signal_value_t<TSignal>> value_;
        bool changed_ = false;
    };

    template <typename T, typename U>
    auto dropRepeats(Signal<T, U> signal)
    {
        return wrap(
                DropRepeats<Signal<T, U>>(
                    std::move(signal))
                );
    }

    template <typename T>
    auto dropRepeats(Signal<T, Constant<T>> signal)
    {
        return std::move(signal);
    }

    template <typename T, typename U>
    auto tryDropRepeats(Signal<T, U> sig)
    -> decltype(dropRepeats(std::move(sig)))
    {
        return dropRepeats(std::move(sig));
    }
} // namespace reactive::signal

