#pragma once

#include "reactive/signaltraits.h"
#include "reactive/reactivevisibility.h"

namespace reactive
{
    namespace signal
    {
        class DtSignal;
    }

    template <>
    struct IsSignal<signal::DtSignal> : std::true_type {};
}

namespace reactive::signal
{
    class DtSignal
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
            return btl::just(signal_time_t(0));
        }

        inline UpdateResult updateEnd(FrameInfo const&)
        {
            return btl::just(signal_time_t(0));
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

    static_assert(IsSignal<DtSignal>::value, "DtSignal is not a signal");

    inline DtSignal dt()
    {
        return DtSignal();
    }
} // namespace reactive::signal

