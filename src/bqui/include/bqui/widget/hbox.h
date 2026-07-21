#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

namespace bqui::widget
{
    /** @brief Places its children in a row, left to right.
     *
     * A braced list of widgets is a fixed row; an array built by forEach() is
     * one whose membership changes, and neither is a special case of the other
     * — see layout().
     */
    BQUI_EXPORT AnyWidget hbox(bq::signal::ArraySignal<AnyWidget> widgets);
} // namespace bqui::widget

