#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    /**
     * @brief Place this widget at the @p gravity fraction of the slack its slot
     * leaves, (0, 0) bottom-left and (1, 1) top-right.
     *
     * A stack or grid places every child by gravity, centred by default. A pure
     * box places a child across its axis by gravity only when it is set here,
     * and otherwise at the leading edge; along its axis the box tiles.
     */
    BQUI_EXPORT AnyWidgetModifier setGravity(
            bq::signal::AnySignal<avg::Vector2f> gravity);
}

