#pragma once

#include "widgetmodifier.h"

#include "reactive/signal/signal.h"

#include <avg/rendertree.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setId(Signal<T, avg::UniqueId> id)
    {
        return makeWidgetModifier([](Widget widget, avg::UniqueId const& id)
            {
                auto container = std::make_shared<avg::IdNode>(
                        id,
                        widget.getObb(),
                        widget.getRenderTree().getRoot()
                        );

                return std::move(widget)
                    .setRenderTree(avg::RenderTree(std::move(container)))
                    ;
            },
            std::move(id)
            );
    }
} // namespace reactive::widget


