#pragma once

#include "widgettransformer.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

namespace reactive::widget
{
    inline auto trackFocus(signal::InputHandle<bool> const& handle)
        // -> FactoryMap
    {
        auto f = [handle](auto widget)
        {
            auto anyHasFocus = [](std::vector<KeyboardInput> const& inputs)
                -> bool
            {
                for (auto&& input : inputs)
                    if (input.hasFocus())
                        return true;

                return false;
            };

            auto input = signal::tee(
                    signal::share(widget.getKeyboardInputs()),
                    anyHasFocus, handle);

            return widget::makeWidgetTransformerResult(
                    std::move(widget).setKeyboardInputs(std::move(input))
                    );
        };

        return widget::makeWidgetTransformer(std::move(f));
    }
} // namespace reactive::widget

