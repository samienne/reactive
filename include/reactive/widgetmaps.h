#pragma once

#include "widget/onclick.h"
#include "widget/onhover.h"
#include "widget/onpointermove.h"
#include "widget/onpointerup.h"
#include "widget/onpointerdown.h"
#include "widget/drawbackground.h"
#include "widget/addwidgets.h"
#include "widget/adddrawings.h"
#include "widget/ondraw.h"

#include "clickevent.h"
#include "widgetmap.h"

#include <reactive/stream/collect.h>
#include <reactive/stream/stream.h>

#include <reactive/signal/map.h>
#include <reactive/signal.h>

#include <btl/sequence.h>
#include <btl/bundle.h>

namespace reactive
{
    template <typename T>
    inline auto transform(Signal<avg::Transform, T> t)
    {
        auto tt = btl::cloneOnCopy(std::move(t));

        auto f = [t=std::move(tt)](auto widget)
        {
            auto w = std::move(widget)
                .transform(t->clone())
                ;

            return w;
        };

        //static_assert(IsWidgetMap<decltype(f)>::value, "");

        return mapWidget(std::move(f));
    }

    template <typename TSignalWidget, typename = typename
        std::enable_if
        <
            IsSignalType<TSignalWidget, Widget>::value
        >::type>
    auto reduce(TSignalWidget w2)
    {
        auto w = signal::share(std::move(w2));

        auto drawing = signal::mbind([](auto&& w) {
                return w.getDrawing().clone(); }, w);
        auto areas = signal::mbind([](auto&& w) { return w.getAreas().clone(); }, w);
        auto obb = signal::mbind([](auto&& w) { return w.getObb().clone(); }, w);
        auto keyboardInputs = signal::mbind([](auto&& w) {
                return w.getKeyboardInputs().clone(); }, w);
        auto theme = signal::mbind([](auto&& w) { return w.getTheme().clone(); }, w);

        return makeWidget(
                std::move(drawing),
                std::move(areas),
                std::move(obb),
                std::move(keyboardInputs),
                std::move(theme));
    }

    template <typename T>
    auto setFocusable(Signal<bool, T> focusable)
    {
        return makeWidgetMap<KeyboardInputTag>(
                []
                (std::vector<KeyboardInput> inputs, bool focusable)
                -> std::vector<KeyboardInput>
                {
                    inputs[0] = std::move(inputs[0]).setFocusable(focusable);
                    return inputs;
                },
                std::move(focusable)
                );
    }

    template <typename T>
    auto requestFocus(Signal<bool, T> requestFocus)
    {
        return makeWidgetMap<KeyboardInputTag>(
                []
                (std::vector<KeyboardInput> inputs, bool requestFocus)
                -> std::vector<KeyboardInput>
                {
                    if (!inputs.empty() && (!inputs[0].hasFocus() || requestFocus))
                        inputs[0] = std::move(inputs[0]).requestFocus(requestFocus);

                    return inputs;
                },
                std::move(requestFocus)
                );
    }

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
            return std::move(widget)
                | setFocusable(signal::constant(true))
                //| requestFocus(std::move(*focusRequest))
                | requestFocus(focusRequest->clone())
                ;
        };

        return mapWidget(std::move(f));
    }
} // namespace reactive

