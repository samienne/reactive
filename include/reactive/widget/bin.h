#pragma once

#include "reactive/widgetmap.h"
#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetMap bin(WidgetFactory f,
            Signal<avg::Vector2f> contentSize);
} // reactive::widget

