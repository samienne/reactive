#pragma once

#include "setfocusable.h"
#include "requestfocus.h"
#include "widgettransformer.h"

#include <reactive/signal/map.h>

#include <reactive/stream/collect.h>
#include <reactive/stream/stream.h>

namespace reactive::widget
{
    inline auto focusOn(stream::Stream<bool> stream)
    {
        auto hasValues = [](std::vector<bool> const& v) -> bool
        {
            return !v.empty();
        };

        auto focusRequest = signal::map(
                hasValues,
                stream::collect(std::move(stream))
                );

        auto f = [focusRequest=btl::cloneOnCopy(std::move(focusRequest))]
            (auto widget)
        {
            return makeWidgetTransformerResult(
                    std::move(widget)
                    | setFocusable(signal::constant(true))
                    | requestFocus(focusRequest->clone())
                    );
        };

        return widget::makeWidgetTransformer(std::move(f));
    }
} // namespace reactive::widget

