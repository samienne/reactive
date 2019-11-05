#pragma once

#include "widgettransformer.h"

#include "reactive/signal/share.h"

#include <avg/drawing.h>

namespace reactive::widget
{
    inline auto bindDrawing()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto drawing = signal::share(widget.getDrawing());

            return makeWidgetTransformerResult(
                    std::move(widget).setDrawing(drawing),
                    drawing
                    );
        });
    }

    inline auto grabDrawing()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto drawing = widget.getDrawing();

            auto empty = signal::map(&DrawContext::drawing<>,
                    widget.getDrawContext()
                    );

            return makeWidgetTransformerResult(
                    std::move(widget).setDrawing(std::move(empty)),
                    std::move(drawing)
                    );
        });
    }

} // namespace reactive::widget

