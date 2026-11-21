#include "bqui/widget/anchorkey.h"

#include <atomic>

namespace bqui::widget::detail
{

AnchorId newAnchorId()
{
    // Ids below the start are reserved for predefined keys.
    static std::atomic<AnchorId> next{ 1024 };
    return next.fetch_add(1, std::memory_order_relaxed);
}

} // namespace bqui::widget::detail
