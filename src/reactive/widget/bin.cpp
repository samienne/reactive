#include "widget/bin.h"

#include "widget/addwidgets.h"
#include "widget/binddrawcontext.h"
#include "widget/bindsize.h"
#include "widget/clip.h"
#include "widget/widgettransformer.h"

namespace reactive::widget
{

WidgetTransformer<void> bin(WidgetFactory f, Signal<avg::Vector2f> contentSize)
{
    auto sizeHint = signal::share(f.getSizeHint());

    return makeWidgetTransformer()
        .compose(bindDrawContext(), bindSize())
        .values(std::move(contentSize), std::move(f))
        .bind([](auto drawContext, auto viewSize, auto contentSize,
                    auto f) mutable
        {
            auto cs = signal::share(std::move(contentSize));

            auto t = signal::map([](avg::Vector2f viewSize,
                        avg::Vector2f contentSize)
                    {
                        float offY = contentSize[1] - viewSize[1];
                        return avg::translate(0.0f, -offY);
                    },
                    std::move(viewSize),
                    cs
                    );

            auto w = std::move(f)(
                    std::move(drawContext),
                    std::move(cs)
                    )
                    .transform(std::move(t))
                    ;

            return addWidget(std::move(w))
                .compose(clip())
                ;
        })
        ;
}
} // namespace reactive::widget
