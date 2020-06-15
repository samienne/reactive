#pragma once

#include <btl/option.h>

#include <chrono>

namespace reactive::signal
{
    using signal_time_t = std::chrono::microseconds;
    using UpdateResult = btl::option<signal_time_t>;

    inline UpdateResult min(UpdateResult const& l, UpdateResult const& r)
    {
        if (l.valid() && r.valid())
            return btl::just(std::min(*l, *r));
        else if (r.valid())
            return r;
        else
            return l;
    }
} // reactive::signal

