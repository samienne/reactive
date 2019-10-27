#pragma once

#include "onhover.h"
#include "binddata.h"

namespace reactive::widget
{
    inline REACTIVE_EXPORT auto bindHover()
    {
        auto hover = signal::input(false);

        return makeWidgetTransformer()
            .compose(onHover([handle=std::move(hover.handle)]
                        (HoverEvent const& e) mutable
                        {
                            handle.set(e.hover);
                        }))
            .compose(bindData(std::move(hover.signal)))
            ;
    }

    template <typename T>
    auto bindHover(Signal<avg::Obb, T> obb)
    {
        auto hover = signal::input(false);

        return makeWidgetTransformer()
            .compose(onHover(signal::constant([handle=std::move(hover.handle)]
                            (HoverEvent const& e) mutable
                            {
                                handle.set(e.hover);
                            }), std::move(obb)
                        ))
            .compose(bindData(std::move(hover.signal)))
            ;
    }
} // namespace reactive::widget

