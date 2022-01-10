#pragma once

#include "instancemodifier.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

namespace reactive::widget
{
    inline auto trackObb(signal::InputHandle<avg::Obb> handle)
        //-> FactoryMap
    {
        return makeSharedInstanceSignalModifier([handle=std::move(handle)](auto instance)
            {
                auto obb = signal::map([](Instance const& instance)
                        {
                            return widget.getObb();
                        },
                        instance
                        );

                auto obb2 = signal::tee(std::move(obb), handle);

                return signal::map([](auto instance, auto obb)
                        {
                            return std::move(instance)
                                .setObb(obb)
                                ;
                        },
                        std::move(instance),
                        std::move(obb2)
                        );
            });
    }

} // namespace reactive::widget

