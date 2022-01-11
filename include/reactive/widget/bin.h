#pragma once

#include "instancemodifier.h"
#include "builder.h"

#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    REACTIVE_EXPORT AnyInstanceModifier bin(AnyBuilder f,
            AnySignal<avg::Vector2f> contentSize);
} // reactive::widget

