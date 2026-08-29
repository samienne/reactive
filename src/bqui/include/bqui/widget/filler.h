#pragma once

#include "bqui/widget/widget.h"

#include "bqui/bquivisibility.h"

namespace bqui::widget
{
    /**
     * @brief An empty widget that expands to fill its pure-solver container's
     * leftover space along the container's layout axis.
     *
     * Inside a pure-solver hbox or vbox it takes no size of its own on the
     * layout axis and instead couples to the container's shared flex variable,
     * so every filler in one container splits the slack evenly while the fixed
     * and default-sized siblings hold their sizes. With two fillers each takes
     * half the remaining room; capping one (maxWidth / maxHeight) hands its
     * surplus to the others. Outside a pure-solver region it is inert.
     */
    BQUI_EXPORT AnyWidget filler();

    /**
     * @brief An empty widget that fills horizontally.
     *
     * In a pure-solver hbox it splits the row's leftover space like filler(); in
     * a pure-solver vbox it takes no vertical space and stretches to the column's
     * width. Outside a pure-solver region it grows to fill available horizontal
     * space.
     */
    BQUI_EXPORT AnyWidget hfiller();

    /**
     * @brief An empty widget that fills vertically.
     *
     * In a pure-solver vbox it splits the column's leftover space like filler();
     * in a pure-solver hbox it takes no horizontal space and stretches to the
     * row's height. Outside a pure-solver region it grows to fill available
     * vertical space.
     */
    BQUI_EXPORT AnyWidget vfiller();

    /**
     * @brief An empty widget that fills both axes.
     *
     * In a pure-solver hbox or vbox it behaves as filler() on the layout axis and
     * stretches to fill the cross axis. Outside a pure-solver region it grows to
     * fill available space on both axes.
     */
    BQUI_EXPORT AnyWidget hwfiller();
} // namespace bqui::widget

