#pragma once

#include "bindkeyboardinputs.h"
#include "setkeyboardinputs.h"
#include "widgettransformer.h"

#include <reactive/signal/signal.h>

namespace reactive::widget
{
    template <typename T>
    auto setFocusable(Signal<T, bool> focusable)
    {
        return widget::makeWidgetTransformer()
            .compose(widget::grabKeyboardInputs())
            .values(std::move(focusable))
            .bind([](auto keyboardInputs, auto focusable)
            {
                auto newInputs = signal::map(
                    [](std::vector<KeyboardInput> inputs, bool focusable)
                    -> std::vector<KeyboardInput>
                    {
                        inputs[0] = std::move(inputs[0]).setFocusable(focusable);
                        return inputs;
                    },
                    std::move(keyboardInputs),
                    std::move(focusable)
                    );

                return widget::setKeyboardInputs(std::move(newInputs));
            });
    }

} // namespace reactice::widget

