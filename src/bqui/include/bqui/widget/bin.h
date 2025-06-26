#pragma once

#include "widget.h"

namespace bqui::widget
{
    BQUI_EXPORT AnyWidget bin(AnyWidget f,
            bq::signal::AnySignal<avg::Vector2f> contentSize);
} // bqui::widget

