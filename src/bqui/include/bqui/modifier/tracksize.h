#pragma once

#include "instancemodifier.h"

#include <bq/signal/signal.h>

#include <ase/vector.h>

namespace bqui::modifier
{
    inline auto trackSize(bq::signal::InputHandle<ase::Vector2f> handle)
    {
        return makeSharedInstanceSignalModifier(
            [handle=std::move(handle)](auto instance) mutable
            {
                auto obb = instance.map([](widget::Instance const& w) -> avg::Obb
                        {
                            return w.getObb();
                        });

                auto obb2 = std::move(obb).tee(
                        std::move(handle),
                        std::mem_fn(&avg::Obb::getSize)
                        );

                return merge(std::move(instance), std::move(obb2))
                    .map([](widget::Instance instance, avg::Obb const& obb)
                            -> widget::Instance
                        {
                            return std::move(instance)
                            .setObb(obb);
                        });
            });
    }
}

