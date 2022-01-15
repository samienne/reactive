#pragma once

#include "widget/widget.h"
#include "widget/builder.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::AnyWidget hbox(std::vector<widget::AnyWidget> widgets);

    REACTIVE_EXPORT widget::AnyWidget hbox(
            AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>> widgets
            );
} // namespace reactive

