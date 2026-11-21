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

    /**
     * @brief Lays widgets out in a row like hbox(), lining their text up on a
     * shared baseline.
     *
     * A child that keeps its own height is placed so its baseline meets the
     * row's; one that publishes no baseline (an image, a plain box) aligns its
     * bottom edge there instead. A child that fills the row's height, or has
     * none of its own, fills it as in hbox(). The row is tall enough for the
     * deepest ascent and the deepest descent; a taller row places the aligned
     * children by its own gravity, centred by default. It publishes its
     * baseline, so rows nest and a margin or background around one keeps it.
     *
     * @code
     * baselineHbox({ label("Name:"), bigTitle, button("OK", onClick) })
     * @endcode
     */
    BQUI_EXPORT AnyWidget baselineHbox(
            bq::signal::ArraySignal<AnyWidget> widgets);
} // namespace bqui::widget

