#pragma once

#include <iostream>
#include <functional>
#include <atomic>
#include <stdint.h>

namespace btl
{
class UniqueId
{
public:
    inline explicit UniqueId(uint64_t id) :
        id_(id)
    {
    }

    UniqueId(UniqueId const&) = default;
    UniqueId(UniqueId&&) noexcept = default;

    inline bool operator==(UniqueId const& rhs) const noexcept
    {
        return id_ == rhs.id_;
    }

    inline bool operator!=(UniqueId const& rhs) const noexcept
    {
        return id_ != rhs.id_;
    }

    inline bool operator<(UniqueId const& rhs) const noexcept
    {
        return id_ < rhs.id_;
    }

    inline bool operator>(UniqueId const& rhs) const noexcept
    {
        return id_ > rhs.id_;
    }

    UniqueId& operator=(UniqueId const& rhs) = default;
    UniqueId& operator=(UniqueId&& rhs) noexcept = default;

    size_t hash() const noexcept
    {
        return std::hash<uint64_t>()(id_);
    }

    uint64_t getValue() const noexcept
    {
        return id_;
    }

private:
    uint64_t id_;
};

inline UniqueId makeUniqueId()
{
    static std::atomic<uint64_t> nextId(1);
    return UniqueId(nextId.fetch_add(1, std::memory_order_relaxed));
}

inline std::ostream& operator<<(std::ostream& stream, UniqueId const& id)
{
    return stream << id.getValue();
}

} // namespace btl

namespace std
{
template <>
struct hash<btl::UniqueId>
{
    using argument_type = btl::UniqueId;
    using result_type = size_t;

    size_t operator()(btl::UniqueId const& id) const noexcept
    {
        return id.hash();
    }
};
} // namespace std
