#pragma once

#include "widgettransformer.h"

#include "reactive/keyboardinput.h"

#include "reactive/signal/signal.h"

#include <btl/cloneoncopy.h>

#include <vector>

namespace reactive::widget
{
    template <typename T>
    auto setKeyboardInputs(Signal<T, std::vector<KeyboardInput>> inputs)
    {
        return makeWidgetTransformer(
            [inputs=btl::cloneOnCopy(std::move(inputs))](auto w) mutable
            {
                auto widget = group(std::move(w), std::move(*inputs))
                    .map([](Widget w, std::vector<KeyboardInput> inputs) -> Widget
                            {
                                return std::move(w).setKeyboardInputs(std::move(inputs));
                            });

                return makeWidgetTransformerResult(
                        std::move(widget)
                    );
            });
    }
} // namespace reactive::widget


