#pragma once

#include "widgettransformer.h"

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetTransformer<void> bin(WidgetFactory f,
            AnySignal<avg::Vector2f> contentSize);
} // reactive::widget

