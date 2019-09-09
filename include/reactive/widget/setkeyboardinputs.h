#pragma once

#include "reactive/keyboardinput.h"
#include "reactive/signal.h"
#include "reactive/widgetmap.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setKeyboardInputs(Signal<std::vector<KeyboardInput>, T> inputs)
    {
        return widgetMap([inputs=btl::cloneOnCopy(std::move(inputs))]
            (auto w) mutable
            {
                return std::move(w)
                    .setKeyboardInputs(std::move(*inputs))
                    ;
            });
    }
} // namespace reactive::widget


