#include "id_counter.h"

#include <atomic>

namespace arrange::detail
{

namespace
{

constexpr Id kAutoVariableIdStart = Id{1} << 63;

std::atomic<Id> g_variableCounter{kAutoVariableIdStart};

}  // namespace

Id nextAutoVariableId() noexcept
{
    return g_variableCounter.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace arrange::detail
