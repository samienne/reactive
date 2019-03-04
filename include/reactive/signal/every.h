#pragma once

#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        class BTL_CLASS_VISIBLE Every;
    }

    template <>
    struct IsSignal<signal::Every> : std::true_type {};
}

namespace reactive::signal
{
    class BTL_CLASS_VISIBLE Every
    {
    public:
        BTL_HIDDEN inline Every(signal_time_t phase) :
            phase_(phase)
        {
        }

        BTL_HIDDEN Every(Every&&) noexcept = default;
        BTL_HIDDEN Every& operator=(Every&&) noexcept = default;

        BTL_HIDDEN inline UpdateResult updateBegin(FrameInfo const& frame)
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

        BTL_HIDDEN inline UpdateResult updateEnd(FrameInfo const&)
        {
            return btl::none;
        }

        BTL_HIDDEN inline signal_time_t evaluate() const
        {
            if (changed_)
                return dt_;

            return signal_time_t(0);
        }

        BTL_HIDDEN inline bool hasChanged() const
        {
            return changed_;
        }

        template <typename TCallback>
        BTL_HIDDEN Connection observe(TCallback&&)
        {
            return Connection();
        }

        BTL_HIDDEN inline Annotation annotate() const
        {
            Annotation a;
            a.addNode("every(" + std::to_string(phase_.count()) + "us)");
            return a;
        }

        BTL_HIDDEN inline auto clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN Every(Every const&) = default;
        BTL_HIDDEN Every& operator=(Every const&) = default;

    private:
        signal_time_t phase_;
        signal_time_t dt_ = std::chrono::microseconds(0);
        bool changed_ = false;
    };

    inline auto every(signal_time_t phase) -> Every
    {
        return Every(phase);
    }
} // namespace reactive::signal

BTL_VISIBILITY_POP

