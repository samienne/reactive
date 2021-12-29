#pragma once

#include "widgettransformer.h"

#include "reactive/signal/share.h"

#include <avg/obb.h>

namespace reactive::widget
{
    inline auto bindObb()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto w = signal::share(std::move(widget));
            auto obb = map([](Widget w) //-> avg::Obb const&
                    {
                        return w.getObb();
                    },
                    w);

            return makeWidgetTransformerResult(
                    std::move(w),
                    signal::share(std::move(obb))
                    );

        });
    }

    inline auto grabObb()
    {
        return bindObb();
    }
} // namespace reactive::widget

