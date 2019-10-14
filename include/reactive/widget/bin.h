#pragma once

#include "widgettransform.h"

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetTransform<void> bin(WidgetFactory f,
            Signal<avg::Vector2f> contentSize);
} // reactive::widget

