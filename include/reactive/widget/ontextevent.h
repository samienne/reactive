#pragma once

#include "widgetmodifier.h"

#include <iostream>

namespace reactive::widget
{
    template <typename T, typename U>
    auto onTextEvent(Signal<T, U> handler)
    {
        return makeWidgetModifier([](Instance instance, auto handler)
            {
                auto inputs = instance.getKeyboardInputs();
                for (auto&& input : inputs)
                {
                    input = std::move(input)
                        .onTextEvent(handler);
                }

                return std::move(instance)
                    .setKeyboardInputs(std::move(inputs))
                    ;
            },
            std::move(handler)
            );
    }

    inline auto onTextEvent(KeyboardInput::TextHandler handler)
    {
        return onTextEvent(signal::constant(std::move(handler)));
    }
} // namespace reactive::widget

