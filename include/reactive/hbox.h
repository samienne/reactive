#pragma once

#include "widget/widgetobject.h"
#include "widget/builder.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::AnyBuilder hbox(std::vector<widget::AnyBuilder> widgets);

    REACTIVE_EXPORT widget::AnyBuilder hbox(
            AnySignal<std::vector<widget::WidgetObject>> widgets
            );
} // namespace reactive

