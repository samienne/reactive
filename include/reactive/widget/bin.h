#pragma once

#include "reactive/widgetfactory.h"

#include <btl/visibility.h>

namespace reactive::widget
{
    using BinSizeHintMap = btl::Function<SizeHint(SizeHint)>;
    using BinObbMap = btl::Function<Signal<avg::Obb>(Signal<SizeHint>)>;

    BTL_VISIBLE WidgetFactory bin(WidgetFactory f, BinSizeHintMap sizeHintMap,
            BinObbMap obbMap);
} // reactive::widget

