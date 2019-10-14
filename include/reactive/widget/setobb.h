#pragma once

#include "widgettransform.h"

#include "reactive/signal.h"

#include <avg/obb.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setObb(Signal<avg::Obb, T> obb)
    {
        return makeWidgetTransform(
            [obb=btl::cloneOnCopy(std::move(obb))](auto w) mutable
            {
                return makeWidgetTransformResult(
                        std::move(w).setObb(std::move(*obb))
                        );
            });
    }
} // namespace reactive::widget

