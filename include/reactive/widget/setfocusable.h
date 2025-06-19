#pragma once

#include "instancemodifier.h"

#include <bq/signal/signal.h>

namespace reactive::widget
{
    template <typename T>
    auto setFocusable(signal::Signal<T, bool> focusable)
    {
        return makeInstanceModifier([](Instance instance, bool focusable)
            {
                auto inputs = instance.getKeyboardInputs();
                if (inputs.size() > 0)
                    inputs[0] = std::move(inputs[0]).setFocusable(focusable);

                return std::move(instance)
                    .setKeyboardInputs(std::move(inputs))
                    ;
            },
            std::move(focusable)
            );
    }
} // namespace reactice::widget

