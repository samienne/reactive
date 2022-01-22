#pragma once

#include "widget.h"

namespace reactive::widget
{
    template <typename T>
    auto setParamsObject(Signal<T, BuildParams> params)
    {
        return makeWidgetModifier([](auto widget, auto params)
        {
            return std::invoke(
                    std::move(widget),
                    std::move(params)
                    );
        },
        std::move(params)
        );
    }

} // namespace reactive::widget

