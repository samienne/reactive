#pragma once

#include "settheme.h"
#include "provideparam.h"

namespace reactive::widget
{
    inline auto provideTheme()
    {
        return provideParam<ThemeTag>();
    }
} // namespace reactive::widget

