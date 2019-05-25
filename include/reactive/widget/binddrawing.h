#pragma once

#include "reactive/widgetvalueprovider.h"

#include "signal/share.h"

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
                    btl::pushBack(std::move(data), drawing)
                    );
        });
    }

} // namespace reactive::widget

