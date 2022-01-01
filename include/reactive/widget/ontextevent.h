#pragma once

#include "setkeyboardinputs.h"
#include "bindkeyboardinputs.h"
#include "widgettransformer.h"

#include <iostream>

namespace reactive::widget
{
    template <typename T, typename U>
    auto onTextEvent(Signal<T, U> handler)
    {
        return makeWidgetModifier([](auto widget, auto handler)
            {
                return signal::map([](Widget widget, auto handler)
                    {
                        auto inputs = widget.getKeyboardInputs();
                        for (auto&& input : inputs)
                        {
                            input = std::move(input)
                                .onTextEvent(handler);
                        }

                        return std::move(widget)
                            .setKeyboardInputs(std::move(inputs))
                            ;
                    },
                    std::move(widget),
                    std::move(handler)
                    );
            },
            std::move(handler)
            );
    }

    inline auto onTextEvent(KeyboardInput::TextHandler handler)
    {
        return onTextEvent(signal::constant(std::move(handler)));
    }
} // namespace reactive::widget

