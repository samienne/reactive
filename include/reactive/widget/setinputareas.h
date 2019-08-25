#pragma once

#include "reactive/inputarea.h"
#include "reactive/signal.h"
#include "reactive/widgetmap.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setInputAreas(Signal<std::vector<InputArea>, T> areas)
    {
        return widgetMap([areas=btl::cloneOnCopy(std::move(areas))](auto w) mutable
            {
                return std::move(w)
                    .setAreas(std::move(*areas))
                    ;
            });
    }
} // namespace reactive::widget

