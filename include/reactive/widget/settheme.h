#pragma once

#include "setparams.h"
#include "theme.h"

namespace reactive::widget
{
    struct ThemeTag
    {
        using type = Theme;
        static signal::AnySignal<Theme> const getDefaultValue()
        {
            return signal::constant(Theme());
        };
    };

    template <typename T>
    auto setTheme(signal::Signal<T, Theme> theme)
    {
        return setParams<ThemeTag>(std::move(theme).share());
    }

    inline auto setTheme(Theme theme)
    {
        return setTheme(signal::constant(std::move(theme)));
    }
} // namespace reactive::widget

