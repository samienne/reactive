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
                auto widget = signal::group(std::move(w), std::move(*areas))
                    .map([](Widget w, std::vector<InputArea> areas) -> Widget
                            {
                                return std::move(w).setInputAreas(std::move(areas));
                            });

                return makeWidgetTransformerResult(
                        std::move(widget)
                        );
            });
    }
} // namespace reactive::widget

