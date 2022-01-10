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
        return makeWidgetModifier([](Instance instance, auto areas)
                {
                    return std::move(instance)
                        .setInputAreas(std::move(areas))
                        ;
                },
                std::move(areas)
                );
    }
} // namespace reactive::widget

