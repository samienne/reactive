#pragma once

#include "widget/widgetobject.h"
#include "widget/builder.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::Builder vbox(std::vector<widget::Builder> widgets);

    REACTIVE_EXPORT widget::Builder vbox(
            AnySignal<std::vector<widget::WidgetObject>> widgets
            );
} // namespace reactive

