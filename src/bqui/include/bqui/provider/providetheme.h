#pragma once

#include "provideparam.h"

#include "bqui/modifier/settheme.h"

namespace bqui::provider
{
    inline auto provideTheme()
    {
        return provideParam<modifier::ThemeTag>();
    }
}

