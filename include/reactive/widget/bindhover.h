#pragma once

#include "onhover.h"
#include "binddata.h"

namespace reactive::widget
{
    auto bindHover()
    {
        auto hover = signal::input(false);

        return onHover([handle=std::move(hover.handle)]
            (HoverEvent const& e) mutable
            {
                handle.set(e.hover);
            })
            | bindData(std::move(hover.signal))
            ;
    }
} // namespace reactive::widget

