#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

namespace bqui::widget
{
    BQUI_EXPORT AnyWidget hScrollBar(
            bq::signal::InputHandle<float> handle,
            bq::signal::AnySignal<float> amount,
            bq::signal::AnySignal<float> handleSize);

    BQUI_EXPORT AnyWidget vScrollBar(
            bq::signal::InputHandle<float> handle,
            bq::signal::AnySignal<float> amount,
            bq::signal::AnySignal<float> handleSize);
} // namespace bqui::widget

