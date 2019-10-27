#pragma once

#include "widgettransformer.h"

#include "reactive/keyboardinput.h"

#include "reactive/signal/share.h"

#include <btl/cloneoncopy.h>
#include <btl/pushback.h>

namespace reactive::widget
{
    inline auto bindKeyboardInputs()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto inputs = signal::share(widget.getKeyboardInputs());

            return std::make_pair(
                    std::move(widget).setKeyboardInputs(inputs),
                    btl::cloneOnCopy(std::make_tuple(inputs))
                    );
        });
    }

    inline auto grabKeyboardInputs()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto inputs = std::move(widget.getKeyboardInputs());

            return std::make_pair(
                    std::move(widget).setKeyboardInputs(
                        signal::constant(std::vector<KeyboardInput>())
                        ),
                    btl::cloneOnCopy(std::make_tuple(
                            std::move(inputs)
                            ))
                    );
        });
    }

} // namespace reactive::widget

