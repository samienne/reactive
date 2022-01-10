#pragma once

#include "setfocusable.h"
#include "requestfocus.h"
#include "instancemodifier.h"

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

        return makeInstanceSignalModifier([](auto instance, auto focusRequest)
            {
                return std::move(instance)
                    | setFocusable(signal::constant(true))
                    | requestFocus(std::move(focusRequest))
                    ;
            },
            std::move(focusRequest)
            );
    }
} // namespace reactive::widget

