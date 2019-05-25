#pragma once

#include "bindobb.h"
#include "mapwidgetdata.h"

#include "reactive/widgetvalueprovider.h"

#include "signal/map.h"

namespace reactive::widget
{
    inline auto bindSize()
    {
        return bindObb() >> mapWidgetData([](auto obb)
            {
                return std::make_tuple(
                        signal::map(&avg::Obb::getSize, std::move(obb))
                        );
            });
    }
} // namespace reactive::widget

