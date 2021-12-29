#pragma once

#include "widgettransformer.h"

#include "reactive/inputarea.h"

#include "reactive/signal/share.h"

namespace reactive::widget
{
    inline auto bindInputAreas()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto w = signal::share(std::move(widget));

            auto inputs = signal::map([](Widget w)// -> std::vector<InputArea> const&
                    {
                        return w.getInputAreas();
                    }, w);

            return makeWidgetTransformerResult(
                    std::move(w),
                    std::move(inputs)
                    );
        });
    }

    inline auto grabInputAreas()
    {
        return bindInputAreas();
    }
} // namespace reactive::widget

