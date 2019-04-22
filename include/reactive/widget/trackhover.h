#pragma once

#include "onhover.h"

#include "reactive/signal/inputhandle.h"

namespace reactive::widget
{
    auto trackHover(signal::InputHandle<bool> handle)
    {
        return onHover([handle=std::move(handle)](HoverEvent const& e) mutable
            {
                handle.set(e.hover);
            });
    }

} // namespace reactive::widget

