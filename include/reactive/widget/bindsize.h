#pragma once

#include "bindobb.h"
#include "widgettransformer.h"

#include "reactive/signal/map.h"

namespace reactive::widget
{
    inline auto bindSize()
    {
        return bindObb()
            .bind([](auto obb)
            {
                return provideValues(
                        signal::map(&avg::Obb::getSize, std::move(obb))
                        );
            });
    }
} // namespace reactive::widget

