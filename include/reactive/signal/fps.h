#pragma once

#include "every.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        inline auto fps(int rate) -> decltype(every(signal_time_t(1)))
        {
            assert(rate > 0);
            return every(std::chrono::microseconds(1000000 / rate));
        }
    }
}

BTL_VISIBILITY_POP

