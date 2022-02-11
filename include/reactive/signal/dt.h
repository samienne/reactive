#pragma once

#include "signal.h"
#include "signaltraits.h"
#include "reactive/reactivevisibility.h"

namespace reactive::signal
{
    class DtSignal;

    template <>
    struct IsSignal<DtSignal> : std::true_type {};

    class REACTIVE_EXPORT DtSignal
    {
    public:
        inline DtSignal()
        {
        }

        DtSignal(DtSignal&&) = default;
        DtSignal& operator=(DtSignal&&) = default;

        inline UpdateResult updateBegin(FrameInfo const& frame)
        {
            dt_ = frame.getDeltaTime();
            return std::make_optional(signal_time_t(0));
        }

        inline UpdateResult updateEnd(FrameInfo const&)
        {
            return std::make_optional(signal_time_t(0));
        }

        inline signal_time_t const& evaluate() const
        {
            return dt_;
        }

        inline bool hasChanged() const
        {
            return true;
        }

        template <typename TCallback>
        Connection observe(TCallback&&)
        {
            return Connection();
        }

        inline Annotation annotate() const
        {
            Annotation a;
            a.addNode("dt()");
            return a;
        };

        inline DtSignal clone() const
        {
            return *this;
        }

    private:
        DtSignal(DtSignal const&) = default;
        DtSignal& operator=(DtSignal const&) = default;

    private:
        signal_time_t dt_ = std::chrono::microseconds(0);
    };

    inline auto dt()
    {
        return wrap(DtSignal());
    }
} // namespace reactive::signal

