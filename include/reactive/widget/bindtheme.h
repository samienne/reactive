#pragma once

#include "theme.h"

#include "widgettransform.h"

#include "reactive/signal/share.h"

namespace reactive::widget
{
    inline auto bindTheme()
    {
        return makeWidgetTransform([](auto widget)
        {
            auto theme = signal::share(widget.getTheme());

            return makeWidgetTransformResult(
                    std::move(widget).setTheme(theme),
                    theme
                    );
        });
    }

} // namespace reactive::widget

