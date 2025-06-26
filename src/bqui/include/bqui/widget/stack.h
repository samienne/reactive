#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <vector>

namespace bqui::widget
{
    BQUI_EXPORT AnyWidget stack(std::vector<AnyWidget> widgets);
} // namespace bqui::widget

