#pragma once

#include "widgettransform.h"

#include "reactive/signal/share.h"

#include <avg/obb.h>

namespace reactive::widget
{
    inline auto bindObb()
    {
        return makeWidgetTransform([](auto widget)
        {
            auto obb = signal::share(widget.getObb());

            return makeWidgetTransformResult(
                    std::move(widget).setObb(obb),
                    obb
                    );

        });
    }

    inline auto grabObb()
    {
        return makeWidgetTransform([](auto widget)
        {
            auto obb = widget.getObb();

            return makeWidgetTransformResult(
                    std::move(widget).setObb(signal::constant(avg::Obb())),
                    std::move(obb)
                    );
        });
    }
} // namespace reactive::widget

