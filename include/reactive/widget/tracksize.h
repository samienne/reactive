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
        return makeSharedWidgetSignalModifier([handle=std::move(handle)](auto widget) mutable
            {
                auto obb = signal::map([](Widget const& w) -> avg::Obb
                        {
                            return w.getObb();
                        },
                        widget);

                auto obb2 = signal::tee(
                        std::move(obb),
                        std::mem_fn(&avg::Obb::getSize),
                        std::move(handle)
                        );

                return group(std::move(widget), std::move(obb2))
                    .map([](Widget w, avg::Obb const& obb) -> Widget
                        {
                            return std::move(w)
                            .setObb(obb);
                        });
            });
    }
} // namespace reactive::widget

