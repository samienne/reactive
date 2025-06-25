#pragma once

#include "instancemodifier.h"

#include "bqui/widget/instance.h"

#include <bq/signal/signal.h>

namespace bqui::modifier
{
    template <typename T>
    auto setFocusable(bq::signal::Signal<T, bool> focusable)
    {
        return makeInstanceModifier([](widget::Instance instance, bool focusable)
            {
                auto inputs = instance.getKeyboardInputs();
                if (inputs.size() > 0)
                    inputs[0] = std::move(inputs[0]).setFocusable(focusable);

                return std::move(instance)
                    .setKeyboardInputs(std::move(inputs))
                    ;
            },
            std::move(focusable)
            );
    }
} // namespace reactice::widget

