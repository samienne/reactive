#pragma once

#include "widgetmodifier.h"

#include <avg/color.h>

namespace bqui::modifier
{
    /**
     * @brief The corner radius of a frame() that is given none.
     */
    inline constexpr float frameCornerRadius = 10.0f;

    BQUI_EXPORT AnyWidgetModifier frame(
            bq::signal::AnySignal<float> cornerRadius,
            bq::signal::AnySignal<avg::Color> color
            );

    BQUI_EXPORT AnyWidgetModifier frame(bq::signal::AnySignal<avg::Color> color);

    BQUI_EXPORT AnyWidgetModifier frame(bq::signal::AnySignal<float> cornerRadius);

    BQUI_EXPORT AnyWidgetModifier frame();
}

