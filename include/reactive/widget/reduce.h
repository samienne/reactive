#pragma once

#include "instance.h"

#include <bq/signal/signal.h>

#include <type_traits>

namespace reactive::widget
{
    template <typename TSignalWidget, typename = typename
        std::enable_if
        <
            bq::signal::IsSignalType<TSignalWidget, Instance>::value
        >::type>
    auto reduce(TSignalWidget w2)
    {
        auto w = std::move(w2).share();

        auto renderTree = w.map([](auto && w)
                {
                    return w.getRenderTree();
                }).join();

        auto areas = w.map([](auto&& w) { return w.getInputAreas().clone(); }).join();
        auto obb = w.map([](auto&& w) { return w.getObb().clone(); }).join();

        auto keyboardInputs = w.map([](auto&& w) {
                return w.getKeyboardInputs().clone();
                }).join();

        auto theme = w.map([](auto&& w) { return w.getTheme().clone(); }).join();

        return makeInstance(
                std::move(renderTree),
                std::move(areas),
                std::move(obb),
                std::move(keyboardInputs),
                std::move(theme)
                );
    }
} // namespace reactive::widget

