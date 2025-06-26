#pragma once

#include "instancemodifier.h"

#include "bqui/widget/widget.h"

#include <bq/signal/signal.h>

namespace bqui::modifier
{
    template <typename T>
    auto requestFocus(bq::signal::Signal<T, bool> requestFocus)
    {
        return makeInstanceModifier([](widget::Instance instance, bool requestFocus)
            {
                auto inputs = instance.getKeyboardInputs();
                if (!inputs.empty() && (!inputs[0].hasFocus() || requestFocus))
                    inputs[0] = std::move(inputs[0]).requestFocus(requestFocus);

                return std::move(instance)
                    .setKeyboardInputs(std::move(inputs))
                    ;
            },
            std::move(requestFocus)
            );
    }

}

