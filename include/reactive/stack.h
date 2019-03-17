#pragma once

#include "widgetfactory.h"
#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT WidgetFactory stack(std::vector<WidgetFactory> widgets);
}

