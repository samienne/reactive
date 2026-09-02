#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <bq/signal/signal.h>

#include <string>

namespace bqui::widget
{
    BQUI_EXPORT AnyWidget label(bq::signal::AnySignal<std::string> text);
} // namespace bqui::widget

