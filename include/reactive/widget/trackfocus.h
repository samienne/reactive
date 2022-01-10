#pragma once

#include "widgetmodifier.h"
#include "setkeyboardinputs.h"
#include "instance.h"

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
        return makeSharedWidgetSignalModifier([](auto instance, auto handle)
                {
                    auto input = signal::tee(
                            signal::map(&Instance::getKeyboardInputs, instance),
                            detail::anyHasFocus,
                            std::move(handle)
                            );

                    return std::move(instance)
                        | setKeyboardInputs(std::move(input))
                        ;
                },
                std::move(handle)
                );
    }
} // namespace reactive::widget

