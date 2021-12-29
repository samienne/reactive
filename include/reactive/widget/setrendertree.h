#pragma once

#include "widgettransformer.h"

#include "reactive/signal/group.h"
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
                auto widget = signal::group(std::move(w), std::move(*renderTree))
                    .map([](Widget w, avg::RenderTree renderTree) -> Widget
                            {
                                return std::move(w).setRenderTree(std::move(renderTree));
                            });

                return makeWidgetTransformerResult(
                        std::move(widget)
                        );
            });
    }
} // namespace reactive::widget


