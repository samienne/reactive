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
     * @brief Give this widget the weak natural size a pure-solver leaf carries:
     * @c width==100 and @c height==100, each at the weakest tier.
     *
     * A pure-solver container states only the structure of its layout and no
     * longer sizes its children, so a leaf contributes its own default size on
     * each axis. Any firmer constraint -- a fixed size, a bound, a container's
     * tiling of a filler -- overrides it; it only decides an axis nothing else
     * pinned, keeping the solve well-posed. A no-op outside a pure-solver
     * region.
     *
     * The shipped content leaves (label, text edit, filled/stroked shapes) apply
     * this themselves, so an ordinary layout needs no call. It cannot be baked
     * into makeWidget() -- the leaf primitive lives in a public header that the
     * private layout machinery this default needs must not reach -- so a widget
     * built directly from makeWidget() (a custom leaf) still adds it by hand. A
     * filler() must not: it is deliberately free on the layout axis.
     */
    BQUI_EXPORT AnyWidgetModifier defaultSize();
} // namespace bqui::modifier
