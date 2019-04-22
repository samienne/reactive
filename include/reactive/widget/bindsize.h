#pragma once

#include "reactive/widgetmap.h"

namespace reactive::widget
{
    auto bindSize()
    {
        return forceMapWidget([](auto widget)
        {
            auto obb = signal::share(widget.getObb());

            return std::move(widget)
                .setObb(obb)
                .addData((signal::map(&avg::Obb::getSize, obb)))
                ;
        });
    }
} // namespace reactive::widget

