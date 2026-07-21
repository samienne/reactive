#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

namespace bqui::widget
{
    /** @brief Places its children in a column, top to bottom.
     *
     * A braced list of widgets is a fixed column; an array built by forEach()
     * is one whose membership changes, and neither is a special case of the
     * other — see layout().
     */
    BQUI_EXPORT AnyWidget vbox(bq::signal::ArraySignal<AnyWidget> widgets);
} // namespace bqui::widget

