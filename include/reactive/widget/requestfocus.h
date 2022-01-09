#pragma once

#include "widgetmodifier.h"

#include <reactive/signal/signal.h>

namespace reactive::widget
{
    template <typename T>
    auto requestFocus(Signal<T, bool> requestFocus)
    {
        return makeWidgetModifier([](Widget widget, bool requestFocus)
            {
                auto inputs = widget.getKeyboardInputs();
                if (!inputs.empty() && (!inputs[0].hasFocus() || requestFocus))
                    inputs[0] = std::move(inputs[0]).requestFocus(requestFocus);

                return std::move(widget)
                    .setKeyboardInputs(std::move(inputs))
                    ;
            },
            std::move(requestFocus)
            );
    }

} // namespace reactive::widget

