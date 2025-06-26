#pragma once

#include "instancemodifier.h"

#include "bqui/inputarea.h"

#include <bq/signal/signal.h>

#include <btl/cloneoncopy.h>

#include <vector>

namespace bqui::modifier
{
    template <typename T>
    auto setInputAreas(bq::signal::Signal<T, std::vector<InputArea>> areas)
    {
        return makeInstanceModifier([](widget::Instance instance, auto areas)
                {
                    return std::move(instance)
                        .setInputAreas(std::move(areas))
                        ;
                },
                std::move(areas)
                );
    }
}

