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
        return makeWidgetTransformer()
            .compose(grabKeyboardInputs())
            .values(std::move(handler))
            .bind([](auto inputs, auto handler) mutable
            {
                auto newInputs = signal::map(
                    [](std::vector<KeyboardInput> inputs, auto const& handler)
                    -> std::vector<KeyboardInput>
                    {
                        for (auto&& input : inputs)
                        {
                            input = std::move(input)
                                .onTextEvent(handler);
                        }

                        return inputs;
                    },
                    std::move(inputs),
                    std::move(handler)
                    );

                return setKeyboardInputs(std::move(newInputs));
            });
    }

    inline auto onTextEvent(KeyboardInput::TextHandler handler)
    {
        return onTextEvent(signal::constant(std::move(handler)));
    }
} // namespace reactive::widget

