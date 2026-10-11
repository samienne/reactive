#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

#include <avg/vector.h>

namespace bqui::modifier
{
    /**
     * @brief Pin this widget's width to the given value in a pure-solver region.
     *
     * Sets the min, the max and the natural to the value and clears any flex,
     * so the widget is rigid there; a fill() or growWidth() cannot stretch it
     * past its bounds. A later minWidth() or maxWidth() moves its bound and
     * drags the other along, so the last writer wins.
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
     * A strong lower bound: an unmeetable floor overflows rather than failing
     * the solve. A max below the value is raised to it. A flexible widget stays
     * flexible, its stretch bounded by the floor.
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
     * bounds rather than cancels a flexible widget's stretch. A min above the
     * value is lowered to it.
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
     * @brief In a pure-solver region, give this widget the natural @p size, for a
     * leaf with no measured natural of its own (a bare shape).
     *
     * The natural is only a pull, felt when there is slack: the widget settles
     * at @p size within its bounds, and a fill() grows it from there.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize(avg::Vector2f size);

    /**
     * @brief In a pure-solver region, give this widget the natural @p size,
     * tracking the signal: the signal form of defaultSize().
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize(
            bq::signal::AnySignal<avg::Vector2f> size);

    /**
     * @brief In a pure-solver region, make this widget flexible on both axes, so
     * it takes all the space its slot offers in every direction. The general form
     * of filler() for a content widget. A later fixed size on an axis overrides
     * it there, and it overrides an earlier one.
     *
     * Along a box's layout axis the widget starts from its natural size (its
     * flex basis; a filler has none) and takes a share of the slack left after
     * any sibling free within its bounds, by weight; a max stops it and the
     * others take the rest. Short of space it gives up a share of the deficit by
     * the same weight, stopping at its min or at zero. The flex carries outward,
     * so the box is flexible along its axis, and across it too, bounded by its
     * children's smallest max. In a stack or grid it fills its slot or cell on
     * both axes.
     */
    BQUI_EXPORT AnyWidgetModifier fill();

    /**
     * @brief fill() with an explicit grow weight on both axes: the widget takes a
     * share of the container's slack in proportion to @p weight on top of its
     * natural size, so a grow(2) child grows twice as fast as a grow(1) (or
     * filler) sibling, and shrinks twice as fast when space is short. @c fill()
     * is @c grow(1).
     */
    BQUI_EXPORT AnyWidgetModifier grow(float weight);

    /**
     * @brief In a pure-solver region, make this widget flexible on its width
     * only, the one-axis form of fill().
     *
     * Where the width is the container's layout axis it takes a filler's share
     * of the slack; across a box it fills the box's width. Either way the
     * container becomes flexible on the width. So a widget whose fill direction
     * is its own (a horizontal scroll bar) fills its length in any container. It adds no natural on the width and leaves the height free, so
     * a fixed height stands alongside. A later fixed width overrides it, and it
     * overrides an earlier one.
     */
    BQUI_EXPORT AnyWidgetModifier growWidth();

    /**
     * @brief In a pure-solver region, make this widget fill along its own height,
     * the vertical counterpart of growWidth(): fills its height in any container
     * and leaves the width free.
     */
    BQUI_EXPORT AnyWidgetModifier growHeight();
} // namespace bqui::modifier
