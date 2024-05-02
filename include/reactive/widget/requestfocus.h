#pragma once

#include "instancemodifier.h"

#include <reactive/signal2/signal.h>

namespace reactive::widget
{
    template <typename T>
    auto requestFocus(signal2::Signal<T, bool> requestFocus)
    {
        return makeInstanceModifier([](Instance instance, bool requestFocus)
            {
                auto inputs = instance.getKeyboardInputs();
                if (!inputs.empty() && (!inputs[0].hasFocus() || requestFocus))
                    inputs[0] = std::move(inputs[0]).requestFocus(requestFocus);

                return std::move(instance)
                    .setKeyboardInputs(std::move(inputs))
                    ;
            },
            std::move(requestFocus)
            );
    }

} // namespace reactive::widget

