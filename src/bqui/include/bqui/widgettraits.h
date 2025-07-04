#pragma once

#include "bq/signal/map.h"

#include <avg/obb.h>

#include <vector>

namespace bqui
{
    template <typename TWidget>
    using WidgetDrawContextType =
        std::decay_t<decltype(std::declval<TWidget>().getDrawContext())>;

    template <typename TWidget>
    using WidgetRenderTreeType =
        std::decay_t<decltype(std::declval<TWidget>().getRenderTree())>;

    template <typename TWidget>
    using WidgetAreasType =
        std::decay_t<decltype(std::declval<TWidget>().getInputAreas())>;

    template <typename TWidget>
    using WidgetObbType =
        std::decay_t<decltype(std::declval<TWidget>().getObb())>;

    template <typename TWidget>
    using WidgetSizeType =
        std::decay_t<decltype(std::declval<TWidget>().getSize())>;

    template <typename TWidget>
    using WidgetKeyboardInputsType =
        std::decay_t<decltype(std::declval<TWidget>().getKeyboardInputs())>;

    template <typename TWidget>
    using WidgetThemeType =
        std::decay_t<decltype(std::declval<TWidget>().getTheme())>;

}

