#pragma once

#include "widgetmap.h"

namespace reactive
{
    template <typename TSignalHandler>
    auto onKeyEvent(TSignalHandler handler)
    {
        return makeWidgetMap<KeyboardInputTag>([]
                (auto inputs, auto const& handler)
                -> std::vector<KeyboardInput>
                {
                    for (auto&& input : inputs)
                        input = std::move(input)
                            .onKeyEvent(handler);
                    return inputs;
                },
                std::move(handler)
                );
    }

    inline auto onKeyEvent(KeyboardInput::Handler handler)
    {
        return onKeyEvent(signal::constant(std::move(handler)));
    }
} // namespace reactive

