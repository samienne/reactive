#include "growsizehint.h"

#include "simplesizehint.h"

namespace reactive
{

static_assert(IsSizeHint<GrowSizeHint<SimpleSizeHint>>::value, "");

} // namespace reactive

