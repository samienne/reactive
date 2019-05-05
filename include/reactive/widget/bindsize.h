#pragma once

#include "mapwidgetdata.h"

#include "reactive/widgetvalueprovider.h"

#include "signal/share.h"
#include "signal/map.h"

#include <avg/obb.h>

#include <btl/pushback.h>

namespace reactive::widget
{
    inline auto bindObb()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto obb = signal::share(widget.getObb());

            return std::make_pair(
                    std::move(widget).setObb(obb),
                    btl::pushBack(std::move(data), obb)
                    );
        });
    }

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

