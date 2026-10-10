#pragma once

#include "widgetmodifier.h"

#include "bqui/widget/anchorkey.h"
#include "bqui/widget/layoutspec.h"

#include "bqui/bquivisibility.h"

namespace bqui::modifier
{
    /**
     * @brief Publishes the horizontal anchor @p key at @p anchor on this
     * widget's box, replacing one with the same key.
     *
     * Size words keep the anchor and a margin moves it onto its outer box.
     * Containers drop it unless they define it themselves, so it is read by
     * the modifiers applied after this one, such as modifier::alignAnchor().
     *
     * @code
     * widget::XAnchorKey const divider;
     * auto w = makeWidget()
     *     | modifier::setAnchor(divider,
     *         bq::signal::constant(widget::Anchor{ 0.0f, 40.0f }))
     *     | modifier::alignAnchor(column, divider);
     * @endcode
     */
    BQUI_EXPORT AnyWidgetModifier setAnchor(widget::XAnchorKey key,
            bq::signal::AnySignal<widget::Anchor> anchor);

    /**
     * @brief Publishes the vertical anchor @p key at @p anchor on this
     * widget's box, measured top down, replacing one with the same key.
     *
     * A text-like widget publishes widget::baselineAnchor so a baseline row
     * aligns it.
     */
    BQUI_EXPORT AnyWidgetModifier setAnchor(widget::YAnchorKey key,
            bq::signal::AnySignal<widget::Anchor> anchor);
} // namespace bqui::modifier
