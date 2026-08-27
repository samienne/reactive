#pragma once

#include "bqui/modifier/widgetmodifier.h"

#include "bqui/widget/boxvariables.h"

#include <bq/signal/signal.h>

#include <arrange/constraint.h>

#include <btl/function.h>

#include <vector>

namespace bqui::modifier::detail
{
    /**
     * @brief Which of a pure-solver region's two per-axis solves a leaf
     * constraint joins: the horizontal solve resolves the x-edges, the vertical
     * the y-edges.
     */
    enum class PureAxis
    {
        horizontal,
        vertical
    };

    /**
     * @brief A widget modifier that, when the widget it wraps is built inside a
     * pure-solver region, contributes a per-axis constraint fragment keyed on
     * this widget's own box into the region's solve.
     *
     * The fragment is @p make applied to the box the enclosing container tiles
     * this widget by and to @p value's current value, pushed into the region
     * collector @p axis names. The builder carries that box unchanged from its
     * birth to the container's read, so the fragment names exactly the box the
     * container places, and the leaf's constraint solves alongside the
     * container's relations and the universal weak default. A no-op outside a
     * pure-solver region, where sizing comes from a SizeHint band instead.
     *
     * The fragment is derived only from @p value and the box's plain edge-
     * variable values, never from the collector, so the weak collector handle it
     * is pushed through stays free of a retain cycle.
     */
    AnyWidgetModifier pureConstraintModifier(PureAxis axis,
            bq::signal::AnySignal<float> value,
            btl::Function<std::vector<arrange::Constraint>(
                widget::BoxVariables const&, float)> make);

    /**
     * @brief Applies @p first then @p second as one widget modifier, so a shared
     * band modifier and a pure-solver constraint modifier travel together under
     * one name.
     */
    AnyWidgetModifier composeModifiers(AnyWidgetModifier first,
            AnyWidgetModifier second);
} // namespace bqui::modifier::detail
