#pragma once

#include "instancemodifier.h"

#include "bqui/keyboardinput.h"

#include <bq/signal/signal.h>

#include <btl/cloneoncopy.h>

#include <vector>

namespace bqui::modifier
{
    template <typename T>
    auto setKeyboardInputs(bq::signal::Signal<T, std::vector<KeyboardInput>> inputs)
    {
        return makeInstanceModifier([](widget::Instance instance,
                    std::vector<KeyboardInput> inputs)
                {
                    return std::move(instance)
                        .setKeyboardInputs(std::move(inputs))
                        ;
                },
                std::move(inputs)
                );
    }
}


