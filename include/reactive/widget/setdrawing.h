#pragma once

#include "reactive/signal.h"
#include "reactive/widgetmap.h"

#include <avg/drawing.h>

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setDrawing(Signal<avg::Drawing, T> drawing)
    {
        return widgetMap([drawing=btl::cloneOnCopy(std::move(drawing))](auto w) mutable
            {
                return std::move(w)
                    .setDrawing(std::move(*drawing))
                    ;
            });
    }
} // namespace reactive::widget

