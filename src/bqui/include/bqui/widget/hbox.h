#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

namespace bqui::widget
{
    /**
     * @brief Lays widgets out in a row, left to right.
     *
     * Takes a list of widgets, or the array forEach() makes from a changing
     * list:
     *
     * @code
     * hbox({ label("Name:"), button("OK", onClick) })
     * hbox(forEach(items, key, makeRow))
     * @endcode
     */
    BQUI_EXPORT AnyWidget hbox(bq::signal::ArraySignal<AnyWidget> widgets);
} // namespace bqui::widget

