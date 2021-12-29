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
            auto w = signal::share(std::move(widget));

            auto renderTree = map([](Widget const& w) //-> avg::RenderTree const&
                    {
                        return w.getRenderTree();
                    },
                    w);

            return makeWidgetTransformerResult(
                    std::move(w),
                    std::move(renderTree)
                    );
        });
    }

    inline auto grabRenderTree()
    {
        return bindRenderTree();
    }

} // namespace reactive::widget


