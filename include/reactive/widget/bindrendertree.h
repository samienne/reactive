#pragma once

#include "widgettransformer.h"

#include "reactive/signal/share.h"

#include <avg/drawing.h>

namespace reactive::widget
{
    inline auto bindRenderTree()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto renderTree = signal::share(widget.getRenderTree());

            return makeWidgetTransformerResult(
                    std::move(widget).setRenderTree(renderTree),
                    renderTree
                    );
        });
    }

    inline auto grabRenderTree()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto renderTree = widget.getRenderTree();

            auto empty = signal::constant(avg::RenderTree());

            return makeWidgetTransformerResult(
                    std::move(widget).setRenderTree(std::move(empty)),
                    std::move(renderTree)
                    );
        });
    }

} // namespace reactive::widget


