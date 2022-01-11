#pragma once

#include "widget/builder.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::AnyBuilder stack(std::vector<widget::AnyBuilder> widgets);
}

