#pragma once

#include "reactive/widgetvalueprovider.h"

#include "reactive/inputarea.h"

#include "signal/share.h"

#include <btl/pushback.h>

namespace reactive::widget
{
    inline auto bindInputAreas()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto inputs = signal::share(widget.getInputAreas());

            return std::make_pair(
                    std::move(widget).setAreas(inputs),
                    btl::pushBack(std::move(data), inputs)
                    );
        });
    }

} // namespace reactive::widget

