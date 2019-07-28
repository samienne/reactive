#pragma once

#include "reactive/widgetvalueprovider.h"

#include "signal/share.h"

#include <avg/drawing.h>

#include <btl/pushback.h>

namespace reactive::widget
{
    inline auto bindDrawContext()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto context = widget.getDrawContext();

            return std::make_pair(
                    std::move(widget),
                    btl::pushBack(std::move(data), std::move(context))
                    );
        });
    }

} // namespace reactive::widget

