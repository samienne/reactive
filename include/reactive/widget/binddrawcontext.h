#pragma once

#include "widgettransformer.h"

#include <avg/drawing.h>

namespace reactive::widget
{
    inline auto bindDrawContext()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto context = widget.getDrawContext();

            return makeWidgetTransformerResult(
                    std::move(widget),
                    std::move(context)
                    );
        });
    }

} // namespace reactive::widget

