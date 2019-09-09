#pragma once

#include "reactive/signal.h"
#include "reactive/widgetmap.h"

#include <avg/obb.h>

#include <btl/cloneoncopy.h>

namespace reactive::widget
{
    template <typename T>
    auto setObb(Signal<avg::Obb, T> obb)
    {
        return widgetMap([obb=btl::cloneOnCopy(std::move(obb))](auto w) mutable
            {
                return std::move(w)
                    .setObb(std::move(*obb))
                    ;
            });
    }
} // namespace reactive::widget

