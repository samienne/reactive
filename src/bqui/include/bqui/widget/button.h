#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

namespace bqui::widget
{
    BQUI_EXPORT AnyWidget button(bq::signal::AnySignal<std::string> label,
            bq::signal::AnySignal<std::function<void()>> onClick);

    BQUI_EXPORT AnyWidget button(std::string label,
            bq::signal::AnySignal<std::function<void()>> onClick);
} // namespace bqui::widget

