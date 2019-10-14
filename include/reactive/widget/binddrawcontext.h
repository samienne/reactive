#pragma once

#include "widgettransform.h"

#include <avg/drawing.h>

namespace reactive::widget
{
    inline auto bindDrawContext()
    {
        return makeWidgetTransform([](auto widget)
        {
            auto context = widget.getDrawContext();

            return makeWidgetTransformResult(
                    std::move(widget),
                    std::move(context)
                    );
        });
    }

} // namespace reactive::widget

