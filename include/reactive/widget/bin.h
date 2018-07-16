#pragma once

#include "reactive/widgetfactory.h"

namespace reactive::widget
{
    using BinSizeHintMap = btl::Function<SizeHint(SizeHint)>;
    using BinObbMap = btl::Function<avg::Obb(SizeHint)>;

    WidgetFactory bin(WidgetFactory f, BinSizeHintMap sizeHintMap,
            BinObbMap obbMap);
} // reactive::widget

