#pragma once

#include <chrono>
#include <optional>
#include <algorithm>

namespace reactive::signal2
{
    using signal_time_t = std::chrono::microseconds;
    using UpdateResult = std::optional<signal_time_t>;

    inline UpdateResult min(UpdateResult const& l, UpdateResult const& r)
    {
        if (l.has_value() && r.has_value())
            return std::min(*l, *r);
        else if (r.has_value())
            return r;
        else
            return l;
    }
} // reactive::signal2

