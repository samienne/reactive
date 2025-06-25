#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <vector>

namespace bqui::widget
{
    BQUI_EXPORT AnyWidget vbox(std::vector<AnyWidget> widgets);

    BQUI_EXPORT AnyWidget vbox(
            bq::signal::AnySignal<std::vector<std::pair<size_t, AnyWidget>>> widgets
            );
} // namespace reactive

