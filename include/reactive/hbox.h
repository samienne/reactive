#pragma once

#include "widgetfactory.h"

#include <btl/visibility.h>

#include <vector>

namespace reactive
{
    BTL_VISIBLE WidgetFactory hbox(std::vector<WidgetFactory> widgets);
} // namespace reactive

