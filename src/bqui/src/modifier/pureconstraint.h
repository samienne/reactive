#pragma once

#include "bqui/modifier/widgetmodifier.h"

#include <bq/signal/signal.h>

#include <arrange/strength.h>

namespace bqui::modifier::detail
{
    /**
     * @brief Which of a pure-solver region's two per-axis solves a band or
     * relation joins: the horizontal solve resolves the x-edges, the vertical
     * the y-edges.
     */
    enum class PureAxis
    {
        horizontal,
        vertical
    };

    /**
     * @brief A widget modifier that, inside a pure-solver region, replaces the
     * natural (preferred) extent band on @p axis with @p value at @p strength.
     *
     * The band lives by name on the builder's descriptor, so a later natural
     * write on the same axis overrides this one outright rather than competing
     * with it. A no-op outside a pure-solver region, where sizing comes from a
     * SizeHint band instead.
     */
    AnyWidgetModifier pureNaturalModifier(PureAxis axis,
            arrange::Strength strength, bq::signal::AnySignal<float> value);

    /**
     * @brief A widget modifier that, inside a pure-solver region, replaces the
     * required lower-bound band on @p axis with @p value. A no-op outside one.
     */
    AnyWidgetModifier pureMinModifier(PureAxis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief A widget modifier that, inside a pure-solver region, replaces the
     * required upper-bound band on @p axis with @p value. A no-op outside one.
     */
    AnyWidgetModifier pureMaxModifier(PureAxis axis,
            bq::signal::AnySignal<float> value);

    /**
     * @brief A widget modifier that, inside a pure-solver region, wraps the
     * builder's descriptor in a fresh outer box inset by @p amount on every
     * edge — the solver half of a margin. A no-op outside a pure-solver region;
     * the build-time inset placement is the wrapper's other half.
     */
    AnyWidgetModifier pureInsetModifier(bq::signal::AnySignal<float> amount);

    /**
     * @brief Applies @p first then @p second as one widget modifier, so a shared
     * band modifier and a pure-solver constraint modifier travel together under
     * one name.
     */
    AnyWidgetModifier composeModifiers(AnyWidgetModifier first,
            AnyWidgetModifier second);
} // namespace bqui::modifier::detail
