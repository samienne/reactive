#pragma once

#include "widgetmodifier.h"

#include "reactive/signal/signal.h"

#include <avg/obb.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setObb(Signal<T, avg::Obb> obb)
    {
        return makeWidgetModifier([](Widget widget, avg::Obb const& obb)
                {
                    std::move(widget)
                        .setObb(obb)
                        ;
                },
                std::move(obb)
                );
    }
} // namespace reactive::widget

