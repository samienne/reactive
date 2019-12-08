#pragma once

#include "signaltraits.h"

namespace reactive::signal
{
    template <typename TSig>
    UpdateResult update(TSig& sig,
            reactive::signal::FrameInfo const& frame)
    {
        auto r = sig.updateBegin(frame);
        auto r2 = sig.updateEnd(frame);

        return reactive::signal::min(r, r2);
    }
} // reactive::signal

