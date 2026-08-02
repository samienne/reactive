#pragma once

#include <chrono>
#include <optional>
#include <algorithm>

namespace bq::signal
{
    using signal_time_t = std::chrono::microseconds;
    struct UpdateResult
    {
        bool didChange = false;
    };

    inline UpdateResult operator+(UpdateResult const& a, UpdateResult const& b)
    {
        return {
            a.didChange || b.didChange
        };
    }

} // bq::signal

