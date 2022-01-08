#pragma once

#include "widgetmodifier.h"
#include "setkeyboardinputs.h"
#include "widget.h"

#include <reactive/signal/tee.h>
#include <reactive/signal/inputhandle.h>

namespace reactive::widget
{
    namespace detail
    {
        inline bool anyHasFocus(std::vector<KeyboardInput> const& inputs)
        {
            for (auto&& input : inputs)
                if (input.hasFocus())
                    return true;

            return false;
        };
    } // namespace detail

    inline auto trackFocus(signal::InputHandle<bool> const& handle)
        // -> FactoryMap
    {
        return makeSharedWidgetSignalModifier([](auto widget, auto handle)
                {
                    auto input = signal::tee(
                            signal::map(&Widget::getKeyboardInputs, widget),
                            detail::anyHasFocus,
                            std::move(handle)
                            );

                    return std::move(widget)
                        | setKeyboardInputs(std::move(input))
                        ;
                },
                std::move(handle)
                );
    }
} // namespace reactive::widget

