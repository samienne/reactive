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
            auto w = signal::share(std::move(widget));

            auto theme = signal::map([](Widget const& w) //-> widget::Theme const&
                    {
                        return w.getTheme();
                    },
                    w);

            return makeWidgetTransformerResult(
                    std::move(w),
                    signal::share(std::move(theme))
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

