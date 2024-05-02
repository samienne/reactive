#pragma once

#include "instancemodifier.h"

#include "reactive/signal2/signal.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setRenderTree(signal2::Signal<T, avg::RenderTree> renderTree)
    {
        return makeInstanceModifier([](Instance instance, avg::RenderTree renderTree)
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


