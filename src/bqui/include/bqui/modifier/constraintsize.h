#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    /**
     * @brief Make this widget absorb the slack width its container leaves along
     * the main axis, so a spacer or field fills the remaining room.
     *
     * The pure-solver counterpart of growWeight, split per axis because a
     * pure-solver leaf constrains its own box and its container's cross axis is
     * uncapped, where an axis-agnostic fill would run away. It contributes a soft
     * pull toward a large width, above the weak 100 default so a filled child
     * grows in preference to an untagged one, yet below the container's own weak
     * fill so it settles at the room the container leaves rather than overflowing
     * it. A no-op outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier fillWidth();

    /**
     * @brief Make this widget absorb the slack height its container leaves along
     * the main axis, the vertical counterpart of fillWidth().
     */
    BQUI_EXPORT AnyWidgetModifier fillHeight();
} // namespace bqui::modifier
