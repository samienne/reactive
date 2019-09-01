#pragma once

#include "reactive/widgetvalueprovider.h"

#include "reactive/keyboardinput.h"

#include "reactive/signal/share.h"

#include <btl/cloneoncopy.h>
#include <btl/pushback.h>

namespace reactive::widget
{
    inline auto bindKeyboardInputs()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto inputs = signal::share(widget.getKeyboardInputs());

            return std::make_pair(
                    std::move(widget).setKeyboardInputs(inputs),
                    btl::cloneOnCopy(btl::pushBack(std::move(data), inputs))
                    );
        });
    }

    inline auto grabKeyboardInputs()
    {
        return widgetValueProvider([](auto widget, auto data)
        {
            auto inputs = std::move(widget.getKeyboardInputs());

            return std::make_pair(
                    std::move(widget).setKeyboardInputs(
                        signal::constant(std::vector<KeyboardInput>())
                        ),
                    btl::cloneOnCopy(btl::pushBack(
                            std::move(data),
                            std::move(inputs)
                            ))
                    );
        });
    }

} // namespace reactive::widget

