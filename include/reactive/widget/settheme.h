#pragma once

#include "setparams.h"
#include "theme.h"

namespace reactive::widget
{
    struct ThemeTag
    {
        using type = Theme;
        static signal2::AnySignal<Theme> const getDefaultValue()
        {
            return signal2::constant(Theme());
        };
    };

    template <typename T>
    auto setTheme(signal2::Signal<T, Theme> theme)
    {
        return setParams<ThemeTag>(std::move(theme).share());
    }

    inline auto setTheme(Theme theme)
    {
        return setTheme(signal2::constant(std::move(theme)));
    }
} // namespace reactive::widget

