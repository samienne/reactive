#pragma once

#include "widget/widget.h"

#include "reactivevisibility.h"

#include <vector>

namespace reactive
{
    REACTIVE_EXPORT widget::AnyWidget vbox(std::vector<widget::AnyWidget> widgets);

    REACTIVE_EXPORT widget::AnyWidget vbox(
            signal2::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>> widgets
            );
} // namespace reactive

