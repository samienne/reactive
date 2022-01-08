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
        return makeWidgetModifier([](Widget widget, std::vector<KeyboardInput> inputs)
                {
                    return std::move(widget)
                        .setKeyboardInputs(std::move(inputs))
                        ;
                },
                std::move(inputs)
                );
    }
} // namespace reactive::widget


