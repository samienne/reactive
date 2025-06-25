#include "bqui/widget/box.h"

namespace bqui::widget
{

static_assert(IsSizeHint<AccumulateSizeHint<Axis::x,
        std::vector<SizeHint>>>::value, "");

}

