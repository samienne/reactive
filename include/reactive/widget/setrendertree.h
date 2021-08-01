#pragma once

#include "widgettransformer.h"

#include "reactive/signal/signal.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setRenderTree(Signal<T, avg::RenderTree> renderTree)
    {
        return makeWidgetTransformer(
            [renderTree=btl::cloneOnCopy(std::move(renderTree))](auto w) mutable
            {
                return makeWidgetTransformerResult(
                        std::move(w).setRenderTree(std::move(*renderTree))
                        );
            });
    }
} // namespace reactive::widget


