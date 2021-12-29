#include "widget/bin.h"

#include "widget/addwidgets.h"
#include "widget/bindsize.h"
#include "widget/clip.h"
#include "widget/transform.h"
#include "widget/widgettransformer.h"

namespace reactive::widget
{

WidgetTransformer<void> bin(WidgetFactory f, AnySignal<avg::Vector2f> contentSize)
{
    auto sizeHint = signal::share(f.getSizeHint());

    return makeWidgetTransformer()
        .compose(bindSize())
        .values(std::move(contentSize), std::move(f))
        .bind([](auto viewSize, auto contentSize, WidgetFactory f) mutable
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

            auto w = std::move(f)
                    .map(transform(std::move(t)))
                    (
                     std::move(cs)
                    )
                    ;

            return addWidget(std::move(w))
                .compose(clip())
                ;
        })
        ;
}
} // namespace reactive::widget
