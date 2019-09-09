#pragma once

#include "reactive/widgetvalueprovider.h"

#include "reactive/signal/share.h"

#include <avg/drawing.h>

#include <btl/pushback.h>

namespace reactive::widget
{
    inline auto bindDrawing()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto drawing = signal::share(widget.getDrawing());

            return std::make_pair(
                    std::move(widget).setDrawing(drawing),
                    btl::cloneOnCopy(btl::pushBack(
                            std::move(data),
                            drawing
                            ))
                    );
        });
    }

    inline auto grabDrawing()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto drawing = widget.getDrawing();

            auto empty = signal::map(&DrawContext::drawing<>,
                    widget.getDrawContext()
                    );

            return std::make_pair(
                    std::move(widget).setDrawing(std::move(empty)),
                    btl::cloneOnCopy(btl::pushBack(
                            std::move(data),
                            std::move(drawing)
                            ))
                    );
        });
    }

} // namespace reactive::widget

