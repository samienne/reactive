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
     * @brief Give this widget the natural size a pure-solver leaf carries: its
     * own SizeHint's natural width and height, at a content strength.
     *
     * A pure-solver container states only the structure of its layout and no
     * longer sizes its children, so a leaf contributes its own content size on
     * each axis, bridged from the SizeHint the widget already measures. The
     * content strength sits above the bare fallback but below the cross-fill and
     * any explicit size word, so a fixed size, a bound, a filler, or a
     * container's cross-fill all override it. A no-op outside a pure-solver
     * region, where the SizeHint drives sizing directly.
     *
     * The shipped content leaves (label, text edit, filled/stroked shapes) apply
     * this themselves, so an ordinary layout needs no call. It cannot be baked
     * into makeWidget() -- the leaf primitive lives in a public header that the
     * private layout machinery this default needs must not reach -- so a widget
     * built directly from makeWidget() (a custom leaf) still adds it by hand. A
     * filler() must not: it is deliberately free on the layout axis.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize();

    /**
     * @brief Give this widget a fixed natural size in a pure-solver region, at
     * the same content strength as defaultSize().
     *
     * The disposition for a leaf whose measured SizeHint is not a sensible pure
     * natural -- a bare shape, whose banded "fill everything" hint would read as
     * an absurd ~10000 natural. It settles at @p size unless a fixed size, a
     * bound or a filler/fill() overrides it. A no-op outside a pure-solver region.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize(avg::Vector2f size);

    /**
     * @brief Make this widget flexible on its container's layout axis in a
     * pure-solver region: it grows to take a share of the container's leftover
     * space, exactly as a filler() does, while keeping its content as a
     * flex-basis for the container's aggregation.
     *
     * The general form of filler() for a content widget. It sets the pure flex
     * band and couples the widget's extent to the container's shared flex
     * variable, so it splits the slack with its filler and fill() siblings; its
     * content natural is dropped only under that distribution. Because flex is a
     * named band field, a later fixed size on the same axis overrides it. A no-op
     * outside a pure-solver region. Fills only the container's layout axis (the
     * one a filler fills); the cross axis keeps its content size.
     */
    BQUI_EXPORT AnyWidgetModifier fill();

    /**
     * @brief fill() with an explicit grow weight: the widget takes a share of the
     * container's slack in proportion to @p weight, so a grow(2) child grows
     * twice as fast as a grow(1) (or filler) sibling. @c fill() is @c grow(1).
     */
    BQUI_EXPORT AnyWidgetModifier grow(float weight);
} // namespace bqui::modifier
