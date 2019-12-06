#pragma once

#include "widget/widgetobject.h"
#include "widgetfactory.h"
#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT WidgetFactory hbox(std::vector<WidgetFactory> widgets);

    REACTIVE_EXPORT WidgetFactory hbox(
            AnySignal<std::vector<widget::WidgetObject>> widgets
            );
} // namespace reactive

