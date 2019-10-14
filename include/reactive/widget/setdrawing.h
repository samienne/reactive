#pragma once

#include "reactive/signal.h"

#include "widgettransform.h"

#include <avg/drawing.h>

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setDrawing(Signal<avg::Drawing, T> drawing)
    {
        return makeWidgetTransform(
            [drawing=btl::cloneOnCopy(std::move(drawing))](auto w) mutable
            {
                return makeWidgetTransformResult(
                        std::move(w).setDrawing(std::move(*drawing))
                        );
            });
    }
} // namespace reactive::widget

