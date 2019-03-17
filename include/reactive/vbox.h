#pragma once

#include "widgetfactory.h"
#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT WidgetFactory vbox(std::vector<WidgetFactory> widgets);
} // namespace reactive

