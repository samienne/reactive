#pragma once

#include "shape.h"

#include "bqui/bquivisibility.h"

namespace bqui::shape
{
    // A circle of diameter min(width, height), centered in the drawn area.
    BQUI_EXPORT AnyShape circle();
}
