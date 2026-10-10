#include "signal/datacontext.h"

#include <btl/uniqueid.h>

#include <algorithm>
#include <cstdint>

namespace bq::signal
{
btl::UniqueId makeUniqueId()
{
    static std::atomic<uint64_t> nextId(1);
    return btl::UniqueId(nextId.fetch_add(1, std::memory_order_relaxed));
}

namespace
{
    std::size_t const minPruneThreshold = 16;
} // anonymous namespace

DataContext::DataContext() :
    id_(btl::makeUniqueId()),
    pruneThreshold_(minPruneThreshold)
{
}

btl::UniqueId DataContext::getId() const
{
    return id_;
}

std::size_t DataContext::getEntryCount() const
{
    return data_.size();
}

void DataContext::pruneExpired()
{
    for (auto i = data_.begin(); i != data_.end();)
    {
        if (i->second.expired())
        {
            i = data_.erase(i);
        }
        else
        {
            ++i;
        }
    }

    // Doubling the live count keeps the sweep amortized constant per insert.
    pruneThreshold_ = std::max(minPruneThreshold, 2 * data_.size());
}

} // namespace bq::signal

