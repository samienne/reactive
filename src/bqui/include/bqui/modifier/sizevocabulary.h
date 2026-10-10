#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

#include <avg/vector.h>

namespace bqui::modifier
{
    /**
     * @brief Raise a widget's minimum width to at least the given value.
     *
     * Additive: the modifier only tightens the width band, never loosens it.
     * Composing several keeps the largest bound and the result does not depend
     * on the order they are applied. The natural size and maximum are pulled up
     * with the minimum only as far as needed to keep min <= natural <= max.
     */
    BQUI_EXPORT AnyWidgetModifier widthAtLeast(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier widthAtLeast(float width);

    /**
     * @brief Lower a widget's maximum width to at most the given value.
     *
     * Additive: the modifier only tightens the width band. Composing several
     * keeps the smallest bound, order-independently. The natural size and
     * minimum are pushed down with the maximum only as far as needed to keep
     * min <= natural <= max.
     */
    BQUI_EXPORT AnyWidgetModifier widthAtMost(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier widthAtMost(float width);

    /**
     * @brief Pin a widget's width, setting minimum, natural and maximum equal.
     */
    BQUI_EXPORT AnyWidgetModifier widthExactly(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier widthExactly(float width);

    /**
     * @brief Raise a widget's minimum height to at least the given value.
     *
     * The height counterpart of widthAtLeast.
     */
    BQUI_EXPORT AnyWidgetModifier heightAtLeast(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier heightAtLeast(float height);

    /**
     * @brief Lower a widget's maximum height to at most the given value.
     *
     * The height counterpart of widthAtMost.
     */
    BQUI_EXPORT AnyWidgetModifier heightAtMost(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier heightAtMost(float height);

    /**
     * @brief Pin a widget's height, setting minimum, natural and maximum equal.
     */
    BQUI_EXPORT AnyWidgetModifier heightExactly(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier heightExactly(float height);

    /**
     * @brief Raise both minimum extents to at least the given size.
     *
     * Convenience over widthAtLeast and heightAtLeast on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier sizeAtLeast(bq::signal::AnySignal<avg::Vector2f> size);
    BQUI_EXPORT AnyWidgetModifier sizeAtLeast(avg::Vector2f size);

    /**
     * @brief Lower both maximum extents to at most the given size.
     *
     * Convenience over widthAtMost and heightAtMost on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier sizeAtMost(bq::signal::AnySignal<avg::Vector2f> size);
    BQUI_EXPORT AnyWidgetModifier sizeAtMost(avg::Vector2f size);

    /**
     * @brief Pin both extents, setting minimum, natural and maximum equal.
     *
     * Convenience over widthExactly and heightExactly on both axes at once.
     */
    BQUI_EXPORT AnyWidgetModifier exactSize(bq::signal::AnySignal<avg::Vector2f> size);
    BQUI_EXPORT AnyWidgetModifier exactSize(avg::Vector2f size);

    /**
     * @brief Set a widget's natural width, the size it settles at within its
     * band.
     *
     * A soft target only: it moves the natural size without changing the
     * minimum or maximum, clamped into the band so min <= natural <= max holds.
     */
    BQUI_EXPORT AnyWidgetModifier preferWidth(bq::signal::AnySignal<float> width);
    BQUI_EXPORT AnyWidgetModifier preferWidth(float width);

    /**
     * @brief Set a widget's natural height, the size it settles at within its
     * band.
     *
     * The height counterpart of preferWidth.
     */
    BQUI_EXPORT AnyWidgetModifier preferHeight(bq::signal::AnySignal<float> height);
    BQUI_EXPORT AnyWidgetModifier preferHeight(float height);

    /**
     * @brief Set a widget's filler weight, the share of leftover space it pulls.
     *
     * A weight of zero (the default) leaves the widget at its natural size; a
     * positive weight makes it a filler that grows into a container's surplus,
     * splitting it with its siblings in proportion to the weight (two grow=1
     * children split it equally, a grow=2 child takes twice the surplus of a
     * grow=1 one). The weight is written onto both axes, so the widget fills in
     * whichever direction its container distributes.
     */
    BQUI_EXPORT AnyWidgetModifier growWeight(bq::signal::AnySignal<float> weight);
    BQUI_EXPORT AnyWidgetModifier growWeight(float weight);
}
