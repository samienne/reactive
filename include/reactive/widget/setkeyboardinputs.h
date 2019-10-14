#pragma once

#include "widgettransform.h"

#include "reactive/keyboardinput.h"
#include "reactive/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setKeyboardInputs(Signal<std::vector<KeyboardInput>, T> inputs)
    {
        return makeWidgetTransform(
            [inputs=btl::cloneOnCopy(std::move(inputs))](auto w) mutable
            {
                return makeWidgetTransformResult(
                        std::move(w).setKeyboardInputs(std::move(*inputs))
                    );
            });
    }
} // namespace reactive::widget


