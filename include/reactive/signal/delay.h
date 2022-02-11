#pragma once

#include "signaltraits.h"
#include "signal.h"

#include <btl/demangle.h>

namespace reactive::signal
{
    template <typename T, typename U>
    class Delay;

    template <typename T, typename U>
    struct IsSignal<Delay<T, U>> : std::true_type {};

    template <typename T, typename U>
    class Delay
    {
    public:
        Delay(Signal<T, U>&& signal) :
            signal_(std::move(signal)),
            values_({
                    std::make_optional(btl::clone(signal_->evaluate())),
                    std::nullopt
                    })
        {
        }

        Delay(Delay&&) = default;
        Delay& operator=(Delay&&) = default;

        std::decay_t<T> const& evaluate() const
        {
            return *values_[index_];
        }

        bool hasChanged() const
        {
            return changed_;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            auto r = signal_->updateBegin(frame);

            changed_ = false;
            if (values_[(index_+1)%2].has_value())
            {
                index_ = (index_+1)%2;
                changed_ = true;
            }

            if (changed_)
                return std::make_optional(signal_time_t(0));

            return r;
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = signal_->updateEnd(frame);
            if (signal_->hasChanged())
                values_[(index_+1)%2] = std::make_optional(btl::clone(signal_->evaluate()));
            else
                values_[(index_+1)%2] = std::nullopt;

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
            auto&& n = a.addNode("delay<" + btl::demangle<U>() + ">");
            a.addTree(n, signal_->annotate());
            return a;
        }

        Delay clone() const
        {
            return *this;
        }

    private:
        Delay(Delay const&) = default;
        Delay& operator=(Delay const&) = default;

    private:
        btl::CloneOnCopy<Signal<T, U>> signal_;
        std::optional<std::decay_t<T>> values_[2];
        uint8_t index_ = 0;
        bool changed_ = false;
    };

    template <typename T, typename U>
    auto delay(Signal<T, U> sig)
    {
        return wrap(Delay<T, U>{ std::move(sig) });
    }
} // namespace reactive::signal

