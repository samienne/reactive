#pragma once

#include "widgettransformer.h"
#include "widget.h"

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

            auto w = signal::share(std::move(widget));

            auto input = signal::tee(
                    signal::map(&Widget::getKeyboardInputs, w),
                    anyHasFocus,
                    handle
                    );

            auto w2 = group(std::move(w), std::move(input))
                .map([](Widget w, std::vector<KeyboardInput> inputs) -> Widget
                        {
                            return std::move(w).setKeyboardInputs(std::move(inputs));
                        });

            return widget::makeWidgetTransformerResult(
                    std::move(w2)
                    );
        };

        return widget::makeWidgetTransformer(std::move(f));
    }
} // namespace reactive::widget

