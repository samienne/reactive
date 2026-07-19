#pragma once

#include "avg/avgvisibility.h"

#include <atomic>
#include <cstdint>
#include <iostream>

namespace avg
{
    class AVG_EXPORT UniqueId
    {
    public:
        UniqueId();
        UniqueId(UniqueId const&) = default;

        UniqueId& operator=(UniqueId const&) = default;

        bool operator==(UniqueId const& id) const;
        bool operator!=(UniqueId const& id) const;
        bool operator<(UniqueId const& id) const;
        bool operator>(UniqueId const& id) const;

        AVG_EXPORT friend std::ostream& operator<<(std::ostream& stream,
                UniqueId const& id);

    private:
        uint64_t value_;
        static std::atomic<uint64_t> nextValue_;
    };
} // namespace avg
