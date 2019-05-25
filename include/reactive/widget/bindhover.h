#pragma once

#include "onhover.h"
#include "binddata.h"

namespace reactive::widget
{
    inline REACTIVE_EXPORT auto bindHover()
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

    template <typename T>
    auto bindHover(Signal<avg::Obb, T> obb)
    {
        auto hover = signal::input(false);

        return onHover(signal::constant([handle=std::move(hover.handle)]
            (HoverEvent const& e) mutable
            {
                handle.set(e.hover);
            }), std::move(obb)
            )
            | bindData(std::move(hover.signal))
            ;
    }
} // namespace reactive::widget

