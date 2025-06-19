#pragma once

#include "instancemodifier.h"

#include <bq/signal/signal.h>

#include <ase/vector.h>

namespace reactive::widget
{
    inline auto trackSize(bq::signal::InputHandle<ase::Vector2f> handle)
        //-> BuiderModifier
    {
        return makeSharedInstanceSignalModifier(
            [handle=std::move(handle)](auto instance) mutable
            {
                auto obb = instance.map([](Instance const& w) -> avg::Obb
                        {
                            return w.getObb();
                        });

                auto obb2 = std::move(obb).tee(
                        std::move(handle),
                        std::mem_fn(&avg::Obb::getSize)
                        );

                return merge(std::move(instance), std::move(obb2))
                    .map([](Instance instance, avg::Obb const& obb) -> Instance
                        {
                            return std::move(instance)
                            .setObb(obb);
                        });
            });
    }
} // namespace reactive::widget

