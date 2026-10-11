#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    /**
     * @brief Place this widget at the @p gravity fraction of the slack its slot
     * leaves, (0, 0) bottom-left and (1, 1) top-right.
     *
     * Every container places its children by gravity, centred by default; a
     * box uses it across its axis only, since along it the box tiles.
     */
    BQUI_EXPORT AnyWidgetModifier setGravity(
            bq::signal::AnySignal<avg::Vector2f> gravity);
}

