#pragma once

#include "widgetmodifier.h"

#include "bqui/widget/widget.h"

#include <bq/signal/signal.h>

#include <avg/brush.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier background(widget::AnyWidget bgWidget);

    BQUI_EXPORT AnyWidgetModifier background(
            bq::signal::AnySignal<avg::Brush> brush);

    BQUI_EXPORT AnyWidgetModifier background();
}

