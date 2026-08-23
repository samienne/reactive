#pragma once

#include "widgetmodifier.h"

#include "bqui/widget/guide.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    /** @brief Align this widget's left edge to @p guide.
     *
     * Records that the widget's left edge meets the shared line @p guide names;
     * its container resolves the line and lines up every widget aligned to the
     * same guide. Additive like the sizing vocabulary — a widget may carry
     * several guide alignments at once. The pull is at medium strength, so a
     * guide beats default gravity but yields to a required or strong size bound.
     */
    BQUI_EXPORT AnyWidgetModifier alignLeft(widget::XGuide guide);

    /** @brief Align this widget's right edge to @p guide. */
    BQUI_EXPORT AnyWidgetModifier alignRight(widget::XGuide guide);

    /** @brief Align this widget's horizontal centre to @p guide. */
    BQUI_EXPORT AnyWidgetModifier alignCenterX(widget::XGuide guide);

    /** @brief Align this widget's top edge to @p guide. */
    BQUI_EXPORT AnyWidgetModifier alignTop(widget::YGuide guide);

    /** @brief Align this widget's bottom edge to @p guide. */
    BQUI_EXPORT AnyWidgetModifier alignBottom(widget::YGuide guide);

    /** @brief Align this widget's vertical centre to @p guide. */
    BQUI_EXPORT AnyWidgetModifier alignCenterY(widget::YGuide guide);
} // namespace bqui::modifier
