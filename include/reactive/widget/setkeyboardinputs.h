#pragma once

#include "instancemodifier.h"

#include "reactive/keyboardinput.h"

#include "reactive/signal2/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setKeyboardInputs(signal2::Signal<T, std::vector<KeyboardInput>> inputs)
    {
        return makeInstanceModifier([](Instance instance, std::vector<KeyboardInput> inputs)
                {
                    return std::move(instance)
                        .setKeyboardInputs(std::move(inputs))
                        ;
                },
                std::move(inputs)
                );
    }
} // namespace reactive::widget


