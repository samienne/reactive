#pragma once

#include "widgetmodifier.h"

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidgetModifier bin(WidgetFactory f,
            AnySignal<avg::Vector2f> contentSize);
} // reactive::widget

