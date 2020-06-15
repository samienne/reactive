#pragma once

#include "signal.h"
#include "signaltraits.h"

#include "reactive/reactivevisibility.h"

namespace reactive::signal
{
    class Every;

    template <>
    struct IsSignal<Every> : std::true_type {};

    class REACTIVE_EXPORT Every
    {
    public:
        inline Every(signal_time_t phase) :
            phase_(phase)
        {
        }

        Every(Every&&) noexcept = default;
        Every& operator=(Every&&) noexcept = default;

        inline UpdateResult updateBegin(FrameInfo const& frame)
        {
            while (dt_ >= phase_)
                dt_ -= phase_;

            dt_ += frame.getDeltaTime();

            if (dt_ >= phase_)
            {
                changed_ = true;
                return btl::just(phase_ + phase_ - dt_);
            }

            changed_ = false;

            return btl::just(std::max(signal_time_t(0), phase_ - dt_));
        }

        inline UpdateResult updateEnd(FrameInfo const&)
        {
            return btl::none;
        }

        inline signal_time_t evaluate() const
        {
            if (changed_)
                return dt_;

            return signal_time_t(0);
        }

        inline bool hasChanged() const
        {
            return changed_;
        }

        template <typename TCallback>
        Connection observe(TCallback&&)
        {
            return Connection();
        }

        inline Annotation annotate() const
        {
            Annotation a;
            a.addNode("every(" + std::to_string(phase_.count()) + "us)");
            return a;
        }

        inline Every clone() const
        {
            return *this;
        }

    private:
        Every(Every const&) = default;
        Every& operator=(Every const&) = default;

    private:
        signal_time_t phase_;
        signal_time_t dt_ = std::chrono::microseconds(0);
        bool changed_ = false;
    };

    inline auto every(signal_time_t phase)
    {
        return wrap(Every(phase));
    }
} // namespace reactive::signal

