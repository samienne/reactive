#pragma once

#include "reactive/widgetvalueprovider.h"

#include "reactive/keyboardinput.h"

#include "signal/share.h"

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
                    btl::pushBack(std::move(data), inputs)
                    );
        });
    }

} // namespace reactive::widget

