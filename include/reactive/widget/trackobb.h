#pragma once

#include "widgetmodifier.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

namespace reactive::widget
{
    inline auto trackObb(signal::InputHandle<avg::Obb> handle)
        //-> FactoryMap
    {
        return makeSharedWidgetSignalModifier([handle=std::move(handle)](auto widget)
            {
                auto obb = signal::map([](Widget const& widget)
                        {
                            return widget.getObb();
                        },
                        widget
                        );

                auto obb2 = signal::tee(std::move(obb), handle);

                return signal::map([](auto widget, auto obb)
                        {
                            return std::move(widget)
                                .setObb(obb)
                                ;
                        },
                        std::move(widget),
                        std::move(obb2)
                        );
            });
    }

} // namespace reactive::widget

