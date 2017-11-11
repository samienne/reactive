#pragma once

#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {

    class BTL_CLASS_VISIBLE DtSignal
    {
    public:
        BTL_HIDDEN inline DtSignal()
        {
        }

        BTL_HIDDEN DtSignal(DtSignal&&) = default;
        BTL_HIDDEN DtSignal& operator=(DtSignal&&) = default;

        BTL_HIDDEN inline UpdateResult updateBegin(FrameInfo const& frame)
        {
            dt_ = frame.getDeltaTime();
            return btl::just(signal_time_t(0));
        }

        BTL_HIDDEN inline UpdateResult updateEnd(FrameInfo const&)
        {
            return btl::just(signal_time_t(0));
        }

        BTL_HIDDEN inline signal_time_t const& evaluate() const
        {
            return dt_;
        }

        BTL_HIDDEN inline bool hasChanged() const
        {
            return true;
        }

        template <typename TCallback>
        BTL_HIDDEN Connection observe(TCallback&&)
        {
            return Connection();
        }

        BTL_HIDDEN inline Annotation annotate() const
        {
            Annotation a;
            a.addNode("dt()");
            return a;
        };

        BTL_HIDDEN inline DtSignal clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN DtSignal(DtSignal const&) = default;
        BTL_HIDDEN DtSignal& operator=(DtSignal const&) = default;

    private:
        signal_time_t dt_ = std::chrono::microseconds(0);
    };

    static_assert(IsSignal<DtSignal>::value, "DtSignal is not a signal");

    inline DtSignal dt()
    {
        return DtSignal();
    }

    }
}

BTL_VISIBILITY_POP

