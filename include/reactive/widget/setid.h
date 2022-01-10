#pragma once

#include "instancemodifier.h"

#include "reactive/signal/signal.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setId(Signal<T, avg::UniqueId> id)
    {
        return makeInstanceModifier([](Instance instance, avg::UniqueId const& id)
            {
                auto container = std::make_shared<avg::IdNode>(
                        id,
                        instance.getObb(),
                        instance.getRenderTree().getRoot()
                        );

                return std::move(instance)
                    .setRenderTree(avg::RenderTree(std::move(container)))
                    ;
            },
            std::move(id)
            );
    }
} // namespace reactive::widget


