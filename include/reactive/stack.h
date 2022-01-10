#pragma once

#include "widget/builder.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::Builder stack(std::vector<widget::Builder> widgets);
}

