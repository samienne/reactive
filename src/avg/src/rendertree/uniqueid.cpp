#include "rendertree/uniqueid.h"

#include <atomic>
#include <iostream>

namespace avg
{

std::atomic<uint64_t> UniqueId::nextValue_ = 1;


UniqueId::UniqueId() :
    value_(nextValue_.fetch_add(1, std::memory_order_relaxed))
{
}

bool UniqueId::operator==(UniqueId const& id) const
{
    return value_ == id.value_;
}

bool UniqueId::operator!=(UniqueId const& id) const
{
    return value_ != id.value_;
}

bool UniqueId::operator<(UniqueId const& id) const
{
    return value_ < id.value_;
}

bool UniqueId::operator>(UniqueId const& id) const
{
    return value_ > id.value_;
}

std::ostream& operator<<(std::ostream& stream, UniqueId const& id)
{
    return stream << "id(" << id.value_ << ")";
}

} // namespace avg
