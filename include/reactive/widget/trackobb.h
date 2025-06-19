#pragma once

#include "instancemodifier.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

namespace reactive::widget
{
    inline auto trackObb(bq::signal::InputHandle<avg::Obb> handle)
    {
        return makeSharedInstanceSignalModifier([handle=std::move(handle)](auto instance)
            {
                auto obb = bq::signal::map([](Instance const& instance)
                        {
                            return widget.getObb();
                        },
                        instance
                        );

                auto obb2 = bq::signal::tee(std::move(obb), handle);

                return bq::signal::map([](auto instance, auto obb)
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

