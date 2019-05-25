#include "box.h"

namespace reactive
{

static_assert(IsSizeHint<AccumulateSizeHint<Axis::x,
        std::vector<SizeHint>>>::value, "");

} // namespace reactive

