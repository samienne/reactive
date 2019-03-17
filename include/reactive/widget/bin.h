#pragma once

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

namespace reactive::widget
{
    using BinSizeHintMap = btl::Function<SizeHint(SizeHint)>;
    using BinObbMap = btl::Function<Signal<avg::Obb>(Signal<SizeHint>)>;

    REACTIVE_EXPORT WidgetFactory bin(WidgetFactory f,
            BinSizeHintMap sizeHintMap, BinObbMap obbMap);
} // reactive::widget

