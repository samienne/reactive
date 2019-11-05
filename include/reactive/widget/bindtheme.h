#pragma once

#include "theme.h"

#include "widgettransformer.h"

#include "reactive/signal/share.h"

namespace reactive::widget
{
    inline auto bindTheme()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto theme = signal::share(widget.getTheme());

            return makeWidgetTransformerResult(
                    std::move(widget).setTheme(theme),
                    theme
                    );
        });
    }

} // namespace reactive::widget

