#pragma once

#include "setparams.h"

#include "bqui/theme.h"

namespace bqui::modifier
{
    struct ThemeTag
    {
        using type = Theme;
        static bq::signal::AnySignal<Theme> const getDefaultValue()
        {
            return bq::signal::constant(Theme());
        };
    };

    template <typename T>
    auto setTheme(bq::signal::Signal<T, Theme> theme)
    {
        return setParams<ThemeTag>(std::move(theme).share());
    }

    inline auto setTheme(Theme theme)
    {
        return setTheme(bq::signal::constant(std::move(theme)));
    }
}

