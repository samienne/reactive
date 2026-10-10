#pragma once

#include "widgetmodifier.h"

#include "bqui/widget/anchorkey.h"
#include "bqui/widget/guide.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    /**
     * @brief Aligns this widget's left edge to @p guide.
     *
     * Every point bound to one guide in a region meets on one solved line,
     * which settles at the furthest of their natural positions; the others
     * are pulled out to it. The pull yields to any stated size, so a widget
     * that cannot reach the guide stays put. Bindings add up: a widget may
     * carry several.
     */
    BQUI_EXPORT AnyWidgetModifier alignLeft(widget::XGuide guide);

    /**
     * @brief Aligns this widget's right edge to @p guide.
     */
    BQUI_EXPORT AnyWidgetModifier alignRight(widget::XGuide guide);

    /**
     * @brief Aligns this widget's horizontal centre to @p guide.
     */
    BQUI_EXPORT AnyWidgetModifier alignCenterX(widget::XGuide guide);

    /**
     * @brief Aligns this widget's top edge to @p guide.
     */
    BQUI_EXPORT AnyWidgetModifier alignTop(widget::YGuide guide);

    /**
     * @brief Aligns this widget's bottom edge to @p guide.
     */
    BQUI_EXPORT AnyWidgetModifier alignBottom(widget::YGuide guide);

    /**
     * @brief Aligns this widget's vertical centre to @p guide.
     */
    BQUI_EXPORT AnyWidgetModifier alignCenterY(widget::YGuide guide);

    /**
     * @brief Aligns this widget's baseline to @p guide.
     *
     * A widget that publishes no baseline is not bound.
     */
    BQUI_EXPORT AnyWidgetModifier alignBaseline(widget::YGuide guide);

    /**
     * @brief Aligns this widget's horizontal anchor @p key to @p guide.
     *
     * The anchor is read as it stands where the modifier is applied; a widget
     * without it is not bound.
     */
    BQUI_EXPORT AnyWidgetModifier alignAnchor(widget::XGuide guide,
            widget::XAnchorKey key);

    /**
     * @brief Aligns this widget's vertical anchor @p key to @p guide.
     */
    BQUI_EXPORT AnyWidgetModifier alignAnchor(widget::YGuide guide,
            widget::YAnchorKey key);
} // namespace bqui::modifier
