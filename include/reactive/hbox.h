#pragma once

#include "widgetfactory.h"
#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT WidgetFactory hbox(std::vector<WidgetFactory> widgets);
} // namespace reactive

