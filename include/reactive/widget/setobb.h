#pragma once

#include "widgettransformer.h"

#include "reactive/signal/signal.h"

#include <avg/obb.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setObb(Signal<T, avg::Obb> obb)
    {
        return makeWidgetTransformer(
            [obb=btl::cloneOnCopy(std::move(obb))](auto w) mutable
            {
                return makeWidgetTransformerResult(
                        std::move(w).setObb(std::move(*obb))
                        );
            });
    }
} // namespace reactive::widget

