#pragma once

#include "widgetmodifier.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

#include <ase/vector.h>

namespace reactive::widget
{
    inline auto trackSize(signal::InputHandle<ase::Vector2f> handle)
        //-> FactoryMap
    {
        return makeSharedWidgetSignalModifier([handle=std::move(handle)](auto instance) mutable
            {
                auto obb = signal::map([](Instance const& w) -> avg::Obb
                        {
                            return w.getObb();
                        },
                        instance);

                auto obb2 = signal::tee(
                        std::move(obb),
                        std::mem_fn(&avg::Obb::getSize),
                        std::move(handle)
                        );

                return group(std::move(instance), std::move(obb2))
                    .map([](Instance instance, avg::Obb const& obb) -> Instance
                        {
                            return std::move(instance)
                            .setObb(obb);
                        });
            });
    }
} // namespace reactive::widget

