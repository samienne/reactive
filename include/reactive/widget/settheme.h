#pragma once

#include "setparams.h"
#include "theme.h"

namespace reactive::widget
{
    struct ThemeTag
    {
        using type = Theme;
        static AnySharedSignal<Theme> const getDefaultValue()
        {
            return share(signal::constant(Theme()));
        };
    };

    template <typename T>
    auto setTheme(Signal<T, Theme> theme)
    {
        return setParams<ThemeTag>(share(std::move(theme)));
    }

    inline auto setTheme(Theme theme)
    {
        return setTheme(signal::constant(std::move(theme)));
    }
} // namespace reactive::widget

