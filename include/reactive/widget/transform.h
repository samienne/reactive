#pragma once

#include "widgettransformer.h"

#include <reactive/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    template <typename T>
    inline auto transform(Signal<T, avg::Transform> t)
    {
        auto tt = btl::cloneOnCopy(std::move(t));

        auto f = [t=std::move(tt)](auto widget) mutable
        {
            auto w = group(std::move(widget), std::move(*t))
                .map([](Widget w, avg::Transform const& t) -> Widget
                        {
                            return std::move(w).transform(t);
                        });
                ;

            return widget::makeWidgetTransformerResult(std::move(w));
        };

        return widget::makeWidgetTransformer(std::move(f));
    }
} // namespace reactive::widget

