#pragma once

#include "widgettransformer.h"

#include "reactive/inputarea.h"

#include "reactive/signal/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setInputAreas(Signal<T, std::vector<InputArea>> areas)
    {
        return makeWidgetTransformer(
            [areas=btl::cloneOnCopy(std::move(areas))](auto w) mutable
            {
                return makeWidgetTransformerResult(
                        std::move(w).setAreas(std::move(*areas))
                        );
            });
    }
} // namespace reactive::widget

