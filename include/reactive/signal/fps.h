#pragma once

#include "every.h"

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

