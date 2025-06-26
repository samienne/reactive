#pragma once

#include "widgetmodifier.h"

#include "bqui/sizehint.h"

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier setSizeHint(
            bq::signal::AnySignal<SizeHint>sizeHint);
}

