#pragma once

#include "widget/widget.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::AnyWidget hbox(std::vector<widget::AnyWidget> widgets);

    REACTIVE_EXPORT widget::AnyWidget hbox(
            bq::signal::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>> widgets
            );
} // namespace reactive

