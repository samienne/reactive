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
            auto inputs = signal::share(widget.getInputAreas());

            return std::make_pair(
                    std::move(widget).setAreas(inputs),
                    btl::cloneOnCopy(std::make_tuple(
                            inputs
                            ))
                    );
        });
    }

    inline auto grabInputAreas()
    {
        return makeWidgetTransformer([](auto widget)
        {
            auto inputs = std::move(widget.getInputAreas());

            return std::make_pair(
                    std::move(widget).setAreas(
                        signal::constant(std::vector<InputArea>())
                        ),
                    btl::cloneOnCopy(std::make_tuple(
                            std::move(inputs)
                            ))
                    );
        });
    }
} // namespace reactive::widget

