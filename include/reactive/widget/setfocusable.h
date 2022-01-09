#pragma once

#include "widgetmodifier.h"

#include <reactive/signal/signal.h>

namespace reactive::widget
{
    template <typename T>
    auto setFocusable(Signal<T, bool> focusable)
    {
        return makeWidgetModifier([](Widget widget, bool focusable)
            {
                auto inputs = widget.getKeyboardInputs();
                if (inputs.size() > 0)
                    inputs[0] = std::move(inputs[0]).setFocusable(focusable);

                return std::move(widget)
                    .setKeyboardInputs(std::move(inputs))
                    ;
            },
            std::move(focusable)
            );
    }

} // namespace reactice::widget

