#pragma once

#include "widget/widget.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::AnyWidget stack(std::vector<widget::AnyWidget> widgets);
}

