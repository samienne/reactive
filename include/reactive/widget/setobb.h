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
                auto widget = signal::group(std::move(w), std::move(*obb))
                    .map([](Widget w, avg::Obb const& obb) -> Widget
                            {
                                return std::move(w).setObb(obb);
                            });

                return makeWidgetTransformerResult(
                        std::move(widget)
                        );
            });
    }
} // namespace reactive::widget

