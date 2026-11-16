#pragma once

#include "widgetmodifier.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <avg/color.h>
#include <avg/vector.h>

namespace bqui::modifier
{
    /**
     * @brief The light a shadow boundary casts with, in logical units: the
     * casters moved by offset, blurred by a Gaussian reaching radius and
     * filled with colour, whose alpha is the shadow's opacity.
     */
    struct ShadowLight
    {
        bq::signal::AnySignal<avg::Vector2f> offset;
        bq::signal::AnySignal<float> radius;
        bq::signal::AnySignal<avg::Color> colour;
    };

    /**
     * @brief Paints the shadow of everything inside the widget that has an
     * outline, as one blurred shape under the widget.
     *
     * @code
     * widget::stack({ background, cards | shadowBoundary(light) })
     * @endcode
     *
     * The outlines inside are consumed: overlapping casters darken once,
     * casters inside do not shadow each other, and an outer boundary sees
     * none of them. The shadow lies under all of the widget's content, so a
     * background inside the boundary covers it; put the boundary above the
     * background. The light animates with the widget's animation.
     */
    BQUI_EXPORT AnyWidgetModifier shadowBoundary(ShadowLight light);

    /**
     * @brief shadowBoundary() with a soft shadow falling slightly down.
     */
    BQUI_EXPORT AnyWidgetModifier shadowBoundary();

    /**
     * @brief A shadow boundary around a backdrop boundary: the shadow is
     * painted first and the widget in a backdrop scope over it, so glass
     * inside shows its neighbours' blurred shadows.
     *
     * @code
     * widget::stack({ stripes,
     *         widget::stack({ card, glass }) | shadowAndBackdropBoundary() })
     * @endcode
     */
    BQUI_EXPORT AnyWidgetModifier shadowAndBackdropBoundary(ShadowLight light);

    /**
     * @brief shadowAndBackdropBoundary() with the light of
     * shadowBoundary().
     */
    BQUI_EXPORT AnyWidgetModifier shadowAndBackdropBoundary();
}
