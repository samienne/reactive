#pragma once

#include "widgetmodifier.h"

#include "bqui/shape/shape.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/brush.h>
#include <avg/builtineffects.h>
#include <avg/color.h>
#include <avg/effect.h>
#include <avg/vector.h>

#include <optional>

namespace bqui::modifier
{
    /**
     * @brief The shadow a surface casts: its shape moved by offset, grown
     * outward by spread and filled with brush.
     */
    struct SurfaceShadow
    {
        bq::signal::AnySignal<avg::Vector2f> offset;
        bq::signal::AnySignal<float> spread;
        bq::signal::AnySignal<avg::Brush> brush;
    };

    /**
     * @brief Paints the widget on a surface: a shape in the widget's size,
     * optionally filled, casting a shadow and showing a backdrop effect.
     *
     * @code
     * label("Card")
     *     | margin(20.0f)
     *     | surface(shape::rectangle(constant(12.0f)),
     *             constant(avg::Brush(white)), shadow,
     *             constant(avg::frostedGlass(options)))
     * @endcode
     *
     * The effect paints first, the fill over it and the widget on top, so a
     * translucent fill shows the effect through. A new effect of the same
     * kind animates its parameters; one of another kind replaces it at
     * once. The shadow lies under
     * everything the enclosing stack paints at the surface's level.
     */
    BQUI_EXPORT AnyWidgetModifier surface(
            shape::AnyShape shape,
            std::optional<bq::signal::AnySignal<avg::Brush>> fill,
            std::optional<SurfaceShadow> shadow,
            std::optional<bq::signal::AnySignal<avg::Effect>> effect
            );

    /**
     * @brief Shows a backdrop effect through the shape behind the widget,
     * with no fill and no shadow.
     */
    BQUI_EXPORT AnyWidgetModifier backdrop(
            shape::AnyShape shape,
            bq::signal::AnySignal<avg::Effect> effect
            );

    /**
     * @brief Frosted glass behind the widget, through the shape.
     *
     * @code
     * label("Hi") | frostedGlass(shape::circle(), constant(options))
     * @endcode
     *
     * Options outside the range avg::frostedGlass accepts are clamped into
     * it (avg::clampToValid).
     */
    BQUI_EXPORT AnyWidgetModifier frostedGlass(
            shape::AnyShape shape,
            bq::signal::AnySignal<avg::FrostedGlassOptions> options
            );

    /**
     * @brief Frosted glass behind the widget, through a rounded rectangle
     * with the same corner radius as frame().
     *
     * @code
     * label("Hi") | frostedGlass()
     * @endcode
     */
    BQUI_EXPORT AnyWidgetModifier frostedGlass(
            avg::FrostedGlassOptions options = avg::FrostedGlassOptions()
            );

    /**
     * @brief Tints the backdrop behind the widget, through the shape.
     *
     * A strength outside [0, 1] is clamped into it.
     */
    BQUI_EXPORT AnyWidgetModifier tint(
            shape::AnyShape shape,
            bq::signal::AnySignal<avg::Color> colour,
            bq::signal::AnySignal<float> strength
            );

    /**
     * @brief Tints the backdrop behind the widget, over its whole area.
     *
     * @code
     * widget | tint(constant(avg::Color(0.0f, 0.0f, 0.0f)), constant(0.5f))
     * @endcode
     */
    BQUI_EXPORT AnyWidgetModifier tint(
            bq::signal::AnySignal<avg::Color> colour,
            bq::signal::AnySignal<float> strength
            );
}
