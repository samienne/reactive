#pragma once

#include "settheme.h"
#include "getparam.h"

namespace reactive::widget
{
    inline auto getTheme()
    {
        return getParam<ThemeTag>();
    }
} // namespace reactive::widget

