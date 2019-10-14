#pragma once

#include "onhover.h"
#include "binddata.h"

namespace reactive::widget
{
    inline REACTIVE_EXPORT auto bindHover()
    {
        auto hover = signal::input(false);

        return makeWidgetTransform()
            .provide(onHover([handle=std::move(hover.handle)]
                        (HoverEvent const& e) mutable
                        {
                            handle.set(e.hover);
                        }))
            .provide(bindData(std::move(hover.signal)))
            ;
    }

    template <typename T>
    auto bindHover(Signal<avg::Obb, T> obb)
    {
        auto hover = signal::input(false);

        return makeWidgetTransform()
            .provide(onHover(signal::constant([handle=std::move(hover.handle)]
                            (HoverEvent const& e) mutable
                            {
                                handle.set(e.hover);
                            }), std::move(obb)
                        ))
            .provide(bindData(std::move(hover.signal)))
            ;
    }
} // namespace reactive::widget

