#pragma once

#include "reactive/signaltraits.h"

namespace reactive
{
    namespace signal
    {
        class Every
        {
        public:
            inline Every(signal_time_t phase) :
                phase_(phase)
            {
            }

            Every(Every&&) = default;
            Every& operator=(Every&&) = default;

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

            Annotation annotate() const
            {
                Annotation a;
                a.addNode("every(" + std::to_string(phase_.count()) + "us)");
                return a;
            }

        private:
            Every(Every const&) = default;
            Every& operator=(Every const&) = default;

        private:
            signal_time_t phase_;
            signal_time_t dt_ = std::chrono::microseconds(0);
            bool changed_ = false;
        };

        static_assert(IsSignal<Every>::value, "Every is not a signal");

        auto every(signal_time_t phase) -> Every
        {
            return Every(phase);
        }
    }
}

