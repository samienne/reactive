#pragma once

#include "widget/widgetobject.h"
#include "widgetfactory.h"
#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT WidgetFactory vbox(std::vector<WidgetFactory> widgets);

    REACTIVE_EXPORT WidgetFactory vbox(
            AnySignal<std::vector<widget::WidgetObject>> widgets
            );
} // namespace reactive

