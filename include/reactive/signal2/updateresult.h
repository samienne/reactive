#pragma once

#include <chrono>
#include <optional>
#include <algorithm>

namespace reactive::signal2
{
    using signal_time_t = std::chrono::microseconds;
    struct UpdateResult
    {
        std::optional<signal_time_t> nextUpdate;
        bool didChange = false;
    };

    inline std::optional<signal_time_t> min(std::optional<signal_time_t> const& l,
            std::optional<signal_time_t> const& r)
    {
        if (l.has_value() && r.has_value())
            return std::min(*l, *r);
        else if (r.has_value())
            return r;
        else
            return l;
    }

    inline UpdateResult operator+(UpdateResult const& a, UpdateResult const& b)
    {
        return {
            min(a.nextUpdate, b.nextUpdate),
            a.didChange || b.didChange
        };
    }

} // reactive::signal2

