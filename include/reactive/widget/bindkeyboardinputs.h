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
            auto w = signal::share(std::move(widget));

            auto inputs = signal::map([](Widget const& w) -> std::vector<KeyboardInput> const&
                    {
                        return w.getKeyboardInputs();
                    },
                    w);

            return makeWidgetTransformerResult(
                    std::move(w),
                    signal::share(std::move(inputs))
                    );
        });
    }

    inline auto grabKeyboardInputs()
    {
        return bindKeyboardInputs();
    }

} // namespace reactive::widget

