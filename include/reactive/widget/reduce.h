#pragma once

#include "widget.h"

#include <reactive/signal/mbind.h>
#include <reactive/signal/signaltraits.h>

#include <type_traits>

namespace reactive::widget
{
    template <typename TSignalWidget, typename = typename
        std::enable_if
        <
            signal::IsSignalType<TSignalWidget, Widget>::value
        >::type>
    auto reduce(TSignalWidget w2)
    {
        auto w = signal::share(std::move(w2));

        auto renderTree = signal::mbind([](auto && w)
                {
                    return w.getRenderTree();
                }, w);

        auto areas = signal::mbind([](auto&& w) { return w.getInputAreas().clone(); }, w);
        auto obb = signal::mbind([](auto&& w) { return w.getObb().clone(); }, w);

        auto keyboardInputs = signal::mbind([](auto&& w) {
                return w.getKeyboardInputs().clone();
                }, w);

        auto theme = signal::mbind([](auto&& w) { return w.getTheme().clone(); }, w);

        return makeWidget(
                std::move(renderTree),
                std::move(areas),
                std::move(obb),
                std::move(keyboardInputs),
                std::move(theme)
                );
    }
} // namespace reactive::widget

