#pragma once

#include "widgettransformer.h"

#include <reactive/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    template <typename T>
    inline auto transform(Signal<T, avg::Transform> t)
    {
        auto tt = btl::cloneOnCopy(std::move(t));

        auto f = [t=std::move(tt)](auto widget)
        {
            auto w = std::move(widget)
                .transform(t->clone())
                ;

            return widget::makeWidgetTransformerResult(std::move(w));
        };

        return widget::makeWidgetTransformer(std::move(f));
    }
} // namespace reactive::widget

