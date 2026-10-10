#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/arraysignal.h>

namespace bqui::widget
{
    /**
     * @brief Lays widgets out in a column, top to bottom.
     *
     * Takes a list of widgets, or the array forEach() makes from a changing
     * list:
     *
     * @code
     * vbox({ label("Name:"), button("OK", onClick) })
     * vbox(forEach(items, key, makeRow))
     * @endcode
     *
     * Publishes its first child's baseline, so a column aligns in a
     * baselineHbox() by its first line.
     */
    BQUI_EXPORT AnyWidget vbox(bq::signal::ArraySignal<AnyWidget> widgets);
} // namespace bqui::widget

