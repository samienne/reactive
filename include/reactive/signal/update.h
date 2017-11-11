#pragma once

#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

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

BTL_VISIBILITY_POP

