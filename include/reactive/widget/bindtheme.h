#pragma once

#include "theme.h"

#include "reactive/widgetvalueprovider.h"

#include "signal/share.h"

#include <btl/pushback.h>

namespace reactive::widget
{
    inline auto bindTheme()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto theme = signal::share(widget.getTheme());

            return std::make_pair(
                    std::move(widget).setTheme(theme),
                    btl::cloneOnCopy(btl::pushBack(std::move(data), theme))
                    );
        });
    }

} // namespace reactive::widget

