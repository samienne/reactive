#pragma once

#include "instancemodifier.h"

#include "reactive/inputarea.h"

#include <bq/signal/signal.h>

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setInputAreas(signal::Signal<T, std::vector<InputArea>> areas)
    {
        return makeInstanceModifier([](Instance instance, auto areas)
                {
                    return std::move(instance)
                        .setInputAreas(std::move(areas))
                        ;
                },
                std::move(areas)
                );
    }
} // namespace reactive::widget

