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
     * @brief A filler that takes leftover space when its container lays out
     * horizontally.
     *
     * In a pure-solver hbox it behaves as filler(); in a vbox it stays inert on
     * the cross axis. Outside a pure-solver region its legacy grow SizeHint
     * drives it instead.
     */
    BQUI_EXPORT AnyWidget hfiller();

    /**
     * @brief A filler that takes leftover space when its container lays out
     * vertically.
     *
     * In a pure-solver vbox it behaves as filler(); in an hbox it stays inert on
     * the cross axis. Outside a pure-solver region its legacy grow SizeHint
     * drives it instead.
     */
    BQUI_EXPORT AnyWidget vfiller();

    /**
     * @brief A filler that takes leftover space on whichever axis its container
     * lays out along, exactly as filler(), plus a legacy grow SizeHint for use
     * outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidget hwfiller();
} // namespace bqui::widget

