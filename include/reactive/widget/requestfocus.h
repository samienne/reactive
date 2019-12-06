#pragma once

#include "bindkeyboardinputs.h"
#include "setkeyboardinputs.h"
#include "widgettransformer.h"

#include <reactive/signal.h>

namespace reactive::widget
{
    template <typename T>
    auto requestFocus(Signal<T, bool> requestFocus)
    {
        return widget::makeWidgetTransformer()
            .compose(widget::grabKeyboardInputs())
            .values(std::move(requestFocus))
            .bind([](auto keyboardInputs, auto requestFocus)
            {
                auto newInputs = signal::map(
                    [](std::vector<KeyboardInput> inputs, bool requestFocus)
                    -> std::vector<KeyboardInput>
                    {
                        if (!inputs.empty() && (!inputs[0].hasFocus() || requestFocus))
                            inputs[0] = std::move(inputs[0]).requestFocus(requestFocus);

                        return inputs;
                    },
                    std::move(keyboardInputs),
                    std::move(requestFocus)
                    );

                return widget::setKeyboardInputs(std::move(newInputs));
            });
    }

} // namespace reactive::widget

