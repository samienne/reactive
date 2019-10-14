#pragma once

#include "widgettransform.h"

#include "reactive/inputarea.h"
#include "reactive/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setInputAreas(Signal<std::vector<InputArea>, T> areas)
    {
        return makeWidgetTransform(
            [areas=btl::cloneOnCopy(std::move(areas))](auto w) mutable
            {
                return makeWidgetTransformResult(
                        std::move(w).setAreas(std::move(*areas))
                        );
            });
    }
} // namespace reactive::widget

