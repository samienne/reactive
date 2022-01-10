#pragma once

#include "widget/widgetobject.h"
#include "widget/builder.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::Builder hbox(std::vector<widget::Builder> widgets);

    REACTIVE_EXPORT widget::Builder hbox(
            AnySignal<std::vector<widget::WidgetObject>> widgets
            );
} // namespace reactive

