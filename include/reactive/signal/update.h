#pragma once

#include "reactive/signaltraits.h"

namespace reactive
{
    namespace signal
    {
        template <typename TSig>
        UpdateResult update(TSig& sig,
                reactive::signal::FrameInfo const& frame)
        {
            auto r = sig.updateBegin(frame);
            auto r2 = sig.updateEnd(frame);

            return reactive::signal::min(r, r2);
        }
    } // signal
} // reactive

