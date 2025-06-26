#include "bqui/growsizehint.h"

#include "bqui/simplesizehint.h"

namespace bqui
{

static_assert(IsSizeHint<GrowSizeHint<SimpleSizeHint>>::value, "");

} // namespace bqui

