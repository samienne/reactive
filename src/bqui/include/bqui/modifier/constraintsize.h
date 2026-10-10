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
     * value unless a min or max at the same strength overrides it. It replaces
     * any flex on the width, whether set by an earlier fill() or growWidth() or
     * aggregated up from a flexing child, so the widget no longer stretches there;
     * a later fill() or growWidth() makes it flexible again. A no-op outside a
     * pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier fixedWidth(bq::signal::AnySignal<float> width);

    /**
     * @brief Pin this widget's height to the given value in a pure-solver region,
     * the vertical counterpart of fixedWidth().
     */
    BQUI_EXPORT AnyWidgetModifier fixedHeight(bq::signal::AnySignal<float> height);

    /**
     * @brief Pin both extents to the given size in a pure-solver region.
     *
     * Convenience over fixedWidth and fixedHeight on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier fixedSize(bq::signal::AnySignal<avg::Vector2f> size);

    /**
     * @brief Hold this widget's width at or above the given value in a pure-solver
     * region.
     *
     * A strong lower bound: it clamps content but yields to the window anchor or a
     * contradicting bound, so an unmeetable floor overflows rather than failing
     * the solve. A flexible widget stays flexible, its stretch bounded by the
     * floor. A no-op outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier minWidth(bq::signal::AnySignal<float> width);

    /**
     * @brief Hold this widget's height at or above the given value in a
     * pure-solver region, the vertical counterpart of minWidth().
     */
    BQUI_EXPORT AnyWidgetModifier minHeight(bq::signal::AnySignal<float> height);

    /**
     * @brief Hold both extents at or above the given size in a pure-solver region.
     *
     * Convenience over minWidth and minHeight on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier minSize(bq::signal::AnySignal<avg::Vector2f> size);

    /**
     * @brief Hold this widget's width at or below the given value in a pure-solver
     * region.
     *
     * A strong upper bound: the ceiling counterpart of minWidth(), which likewise
     * bounds rather than cancels a flexible widget's stretch. A no-op outside a
     * pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier maxWidth(bq::signal::AnySignal<float> width);

    /**
     * @brief Hold this widget's height at or below the given value in a
     * pure-solver region, the vertical counterpart of maxWidth().
     */
    BQUI_EXPORT AnyWidgetModifier maxHeight(bq::signal::AnySignal<float> height);

    /**
     * @brief Hold both extents at or below the given size in a pure-solver region.
     *
     * Convenience over maxWidth and maxHeight on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier maxSize(bq::signal::AnySignal<avg::Vector2f> size);

    /**
     * @brief In a pure-solver region, give this widget the fixed natural @p size
     * at content strength, for a leaf whose measured SizeHint is not a sensible
     * pure natural (a bare shape). It settles at @p size unless a fixed size, a
     * bound or a filler/fill() overrides it; a no-op outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize(avg::Vector2f size);

    /**
     * @brief In a pure-solver region, give this widget the natural @p size at
     * content strength, tracking the signal, for a leaf whose measured content
     * size is its own pure natural (a label from its text extents). It settles at
     * @p size unless a fixed size, a bound or a filler/fill() overrides it; a
     * no-op outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize(
            bq::signal::AnySignal<avg::Vector2f> size);

    /**
     * @brief In a pure-solver region, make this widget flexible on its
     * container's layout axis, growing to take a share of the container's
     * leftover space as a filler() does. The general form of filler() for a
     * content widget. A later fixed size on the same axis overrides it, and it
     * overrides an earlier one; a no-op outside a pure-solver region. Fills only
     * the layout axis; the cross axis keeps its content size. A grid has no
     * layout axis, so there it fills its cell (or span) on both axes.
     *
     * The widget starts from its natural size (its flex basis; a filler has
     * none) and takes a share of the slack left after every sibling's natural
     * or fixed size, by weight; a max stops it and the others take the rest.
     * Short of space it gives up a share of the deficit by the same weight,
     * stopping at its min or at zero.
     */
    BQUI_EXPORT AnyWidgetModifier fill();

    /**
     * @brief fill() with an explicit grow weight: the widget takes a share of the
     * container's slack in proportion to @p weight on top of its natural size,
     * so a grow(2) child grows twice as fast as a grow(1) (or filler) sibling,
     * and shrinks twice as fast when space is short. @c fill() is @c grow(1).
     */
    BQUI_EXPORT AnyWidgetModifier grow(float weight);

    /**
     * @brief In a pure-solver region, make this widget fill along its own width,
     * whichever axis its container stacks along.
     *
     * On the width axis it takes a filler's share of the container's slack where
     * that is the layout axis, and is stretched by the container's cross-fill
     * where it is not, so a widget whose fill direction is its own (a horizontal
     * scroll bar) fills its length in any container. It is flexible on the
     * width either way, so its container is too, and adds no natural there; it
     * leaves the height free, so a fixed height stands alongside. A later fixed
     * width overrides it, and it overrides an earlier one. A no-op outside a
     * pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier growWidth();

    /**
     * @brief In a pure-solver region, make this widget fill along its own height,
     * the vertical counterpart of growWidth(): fills its height in any container
     * and leaves the width free.
     */
    BQUI_EXPORT AnyWidgetModifier growHeight();
} // namespace bqui::modifier
