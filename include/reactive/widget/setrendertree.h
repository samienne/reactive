#pragma once

#include "widgetmodifier.h"

#include "reactive/signal/group.h"
#include "reactive/signal/signal.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setRenderTree(Signal<T, avg::RenderTree> renderTree)
    {
        return makeWidgetModifier([](Instance instance, avg::RenderTree renderTree)
                -> Instance
                {
                    return std::move(instance)
                        .setRenderTree(std::move(renderTree))
                        ;
                },
                std::move(renderTree)
                );
    }
} // namespace reactive::widget


