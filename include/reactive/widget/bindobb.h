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
            auto obb = signal::share(widget.getObb());

            return makeWidgetTransformerResult(
                    std::move(widget).setObb(obb),
                    obb
                    );

        });
    }

    inline auto grabObb()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto obb = widget.getObb();

            return makeWidgetTransformerResult(
                    std::move(widget).setObb(signal::constant(avg::Obb())),
                    std::move(obb)
                    );
        });
    }
} // namespace reactive::widget

