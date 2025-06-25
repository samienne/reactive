#pragma once

#include "instancemodifier.h"

#include <bq/signal/signal.h>

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace bqui::modifier
{
    template <typename T>
    auto setRenderTree(bq::signal::Signal<T, avg::RenderTree> renderTree)
    {
        return makeInstanceModifier([](widget::Instance instance,
                    avg::RenderTree renderTree) -> widget::Instance
                {
                    return std::move(instance)
                        .setRenderTree(std::move(renderTree))
                        ;
                },
                std::move(renderTree)
                );
    }
}


