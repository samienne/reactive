#pragma once

#include "instancemodifier.h"

#include "reactive/signal/signal.h"

#include <avg/obb.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setObb(Signal<T, avg::Obb> obb)
    {
        return makeInstanceModifier([](Instance instance, avg::Obb const& obb)
                {
                    std::move(instance)
                        .setObb(obb)
                        ;
                },
                std::move(obb)
                );
    }
} // namespace reactive::widget

