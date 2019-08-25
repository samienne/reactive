#pragma once

#include "reactive/widgetvalueprovider.h"

#include "reactive/signal/share.h"

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

} // namespace reactive::widget

