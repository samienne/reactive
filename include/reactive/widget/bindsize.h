#pragma once

#include "reactive/widgetvalueprovider.h"

#include "signal/share.h"
#include "signal/map.h"

#include <avg/obb.h>

namespace reactive::widget
{
    auto bindSize()
    {
        return widgetValueProvider([](auto widget)
        {
            auto obb = signal::share(widget.getObb());

            return std::move(widget)
                .setObb(obb)
                .addData((signal::map(&avg::Obb::getSize, obb)))
                ;
        });
    }
} // namespace reactive::widget

