#pragma once

#include "widgetmodifier.h"

#include "reactive/keyboardinput.h"

#include "reactive/signal/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setKeyboardInputs(Signal<T, std::vector<KeyboardInput>> inputs)
    {
        return makeWidgetModifier([](Instance instance, std::vector<KeyboardInput> inputs)
                {
                    return std::move(instance)
                        .setKeyboardInputs(std::move(inputs))
                        ;
                },
                std::move(inputs)
                );
    }
} // namespace reactive::widget


