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

    template <typename TFunc>
    inline auto bindThemeColor(TFunc&& f)
    {
        return makeWidgetTransformer()
            .compose(bindTheme())
            .bind([f=std::forward<TFunc>(f)](auto const& theme)
            {
                return provideValues(signal::map(f, theme));
            });
    }
} // namespace reactive::widget

