#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

#include <avg/vector.h>

namespace bqui::modifier
{
    /**
     * @brief Pin this widget's width to the given value in a pure-solver region.
     *
     * A strong equality above the weak 100 default and below a required bound, so
     * the width settles at the value unless a required min or max overrides it. A
     * no-op outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier fixedWidth(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier fixedWidth(float width);

    /**
     * @brief Pin this widget's height to the given value in a pure-solver region,
     * the vertical counterpart of fixedWidth().
     */
    BQUI_EXPORT AnyWidgetModifier fixedHeight(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier fixedHeight(float height);

    /**
     * @brief Pin both extents to the given size in a pure-solver region.
     *
     * Convenience over fixedWidth and fixedHeight on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier fixedSize(bq::signal::AnySignal<avg::Vector2f> size);
    BQUI_EXPORT AnyWidgetModifier fixedSize(avg::Vector2f size);

    /**
     * @brief Hold this widget's width at or above the given value in a pure-solver
     * region.
     *
     * A required lower bound the solve cannot violate. A no-op outside a
     * pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier minWidth(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier minWidth(float width);

    /**
     * @brief Hold this widget's height at or above the given value in a
     * pure-solver region, the vertical counterpart of minWidth().
     */
    BQUI_EXPORT AnyWidgetModifier minHeight(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier minHeight(float height);

    /**
     * @brief Hold both extents at or above the given size in a pure-solver region.
     *
     * Convenience over minWidth and minHeight on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier minSize(bq::signal::AnySignal<avg::Vector2f> size);
    BQUI_EXPORT AnyWidgetModifier minSize(avg::Vector2f size);

    /**
     * @brief Hold this widget's width at or below the given value in a pure-solver
     * region.
     *
     * A required upper bound the solve cannot violate. A no-op outside a
     * pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier maxWidth(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier maxWidth(float width);

    /**
     * @brief Hold this widget's height at or below the given value in a
     * pure-solver region, the vertical counterpart of maxWidth().
     */
    BQUI_EXPORT AnyWidgetModifier maxHeight(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier maxHeight(float height);

    /**
     * @brief Hold both extents at or below the given size in a pure-solver region.
     *
     * Convenience over maxWidth and maxHeight on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier maxSize(bq::signal::AnySignal<avg::Vector2f> size);
    BQUI_EXPORT AnyWidgetModifier maxSize(avg::Vector2f size);

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
