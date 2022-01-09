#pragma once

#include "widgetmodifier.h"

#include "reactive/inputarea.h"

#include "reactive/signal/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setInputAreas(Signal<T, std::vector<InputArea>> areas)
    {
        return makeWidgetModifier([](Widget widget, auto areas)
                {
                    return std::move(widget)
                        .setInputAreas(std::move(areas))
                        ;
                },
                std::move(areas)
                );
    }
} // namespace reactive::widget

