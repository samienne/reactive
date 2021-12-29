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
                        signal::map([](avg::Obb const& obb)
                            {
                                return obb.getSize();
                            }, std::move(obb))
                        );
            });
    }
} // namespace reactive::widget

