#pragma once

#include "widgetmodifier.h"

#include "bqui/clickevent.h"

#include <bq/signal/signal.h>

#include <functional>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier onClick(unsigned int button,
            bq::signal::AnySignal<std::function<void(ClickEvent const&)>> cb);

    BQUI_EXPORT AnyWidgetModifier onClick(unsigned int button,
            bq::signal::AnySignal<std::function<void()>> cb);

    BQUI_EXPORT AnyWidgetModifier onClick(unsigned int button,
            std::function<void(ClickEvent const&)> f);

}

