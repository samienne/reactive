#pragma once

#include "reactive/signal.h"

#include "widgettransformer.h"

#include <avg/drawing.h>

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setDrawing(Signal<T, avg::Drawing> drawing)
    {
        return makeWidgetTransformer(
            [drawing=btl::cloneOnCopy(std::move(drawing))](auto w) mutable
            {
                return makeWidgetTransformerResult(
                        std::move(w).setDrawing(std::move(*drawing))
                        );
            });
    }
} // namespace reactive::widget

