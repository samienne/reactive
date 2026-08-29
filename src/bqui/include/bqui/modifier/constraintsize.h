#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

#include <avg/vector.h>

namespace bqui::modifier
{
    /**
     * @brief Pin this widget's width to the given value in a pure-solver region.
     *
     * A strong equality above the weak 100 default, so the width settles at the
     * value unless a min or max at the same strength overrides it. A no-op outside
     * a pure-solver region.
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
     * A strong lower bound: it clamps content but yields to the window anchor or a
     * contradicting bound, so an unmeetable floor overflows rather than failing
     * the solve. A no-op outside a pure-solver region.
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
     * A strong upper bound: the ceiling counterpart of minWidth(). A no-op outside
     * a pure-solver region.
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
     * @brief In a pure-solver region, size this widget to its own SizeHint's
     * natural width and height (and its SizeHint bounds), so a leaf carries its
     * content size where its container no longer sizes it. A fixed size, a bound,
     * a filler or a cross-fill all override it; a no-op outside a pure-solver
     * region.
     *
     * The shipped content leaves (label, text edit, filled/stroked shapes) apply
     * this themselves, so an ordinary layout needs no call. A widget built
     * directly from makeWidget() (a custom leaf) adds it by hand; a filler() must
     * not, being deliberately free on the layout axis.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize();

    /**
     * @brief In a pure-solver region, give this widget the fixed natural @p size
     * at content strength, for a leaf whose measured SizeHint is not a sensible
     * pure natural (a bare shape). It settles at @p size unless a fixed size, a
     * bound or a filler/fill() overrides it; a no-op outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize(avg::Vector2f size);

    /**
     * @brief In a pure-solver region, make this widget flexible on its
     * container's layout axis, growing to take a share of the container's
     * leftover space as a filler() does. The general form of filler() for a
     * content widget. A later fixed size on the same axis overrides it; a no-op
     * outside a pure-solver region. Fills only the layout axis; the cross axis
     * keeps its content size.
     */
    BQUI_EXPORT AnyWidgetModifier fill();

    /**
     * @brief fill() with an explicit grow weight: the widget takes a share of the
     * container's slack in proportion to @p weight, so a grow(2) child grows
     * twice as fast as a grow(1) (or filler) sibling. @c fill() is @c grow(1).
     */
    BQUI_EXPORT AnyWidgetModifier grow(float weight);
} // namespace bqui::modifier
