#include "signal/datacontext.h"

#include <btl/uniqueid.h>

namespace reactive::signal
{
btl::UniqueId makeUniqueId()
{
    static std::atomic<uint64_t> nextId(1);
    return btl::UniqueId(nextId.fetch_add(1, std::memory_order_relaxed));
}

DataContext::DataContext() :
    id_(btl::makeUniqueId())
{
}

btl::UniqueId DataContext::getId() const
{
    return id_;
}

} // namespace reactive::signal

