#pragma once

#include "widgetfactory.h"

#include <btl/visibility.h>

#include <vector>

namespace reactive
{
    BTL_VISIBLE WidgetFactory stack(std::vector<WidgetFactory> widgets);
}

