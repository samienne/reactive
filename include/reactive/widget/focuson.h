#pragma once

#include "setfocusable.h"
#include "requestfocus.h"
#include "widgetmodifier.h"

#include <reactive/signal/map.h>

#include <reactive/stream/collect.h>
#include <reactive/stream/stream.h>

namespace reactive::widget
{
    namespace detail
    {
        bool hasValues(std::vector<bool> const& v)
        {
            return !v.empty();
        };
    } // namespace detail

    inline auto focusOn(stream::Stream<bool> stream)
    {
        auto focusRequest = signal::map(
                detail::hasValues,
                stream::collect(std::move(stream))
                );

        return makeWidgetSignalModifier([](auto widget, auto focusRequest)
            {
                return std::move(widget)
                    | setFocusable(signal::constant(true))
                    | requestFocus(std::move(focusRequest))
                    ;
            },
            std::move(focusRequest)
            );
    }
} // namespace reactive::widget

