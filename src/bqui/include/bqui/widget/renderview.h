#pragma once

#include "widget.h"

#include "bqui/renderview.h"
#include "bqui/bquivisibility.h"

namespace bqui::widget
{
    /** @brief Shows @p view, stretched over the widget's rectangle.
     *
     * Expands in both directions. The view's pixel size follows the widget's
     * size in window units.
     */
    BQUI_EXPORT AnyWidget renderView(RenderView view);
} // namespace bqui::widget
