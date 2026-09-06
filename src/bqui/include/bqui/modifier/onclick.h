#pragma once

#include "widgetmodifier.h"

#include "bqui/clickevent.h"

#include <bq/signal/signal.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier onClick(unsigned int button,
            bq::signal::AnySignal<void(ClickEvent const&)> cb);

}

