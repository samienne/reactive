#pragma once

#include "instancemodifier.h"

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyInstanceModifier bin(WidgetFactory f,
            AnySignal<avg::Vector2f> contentSize);
} // reactive::widget

