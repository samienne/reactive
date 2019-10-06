#include "widget/bin.h"

#include "widget/binddrawcontext.h"
#include "widget/clip.h"

#include "bindwidgetmap.h"

namespace reactive::widget
{

WidgetMap bin(WidgetFactory f, Signal<avg::Vector2f> contentSize)
{
    auto sizeHint = signal::share(f.getSizeHint());

    return makeWidgetMap()
        .provide(bindDrawContext(), bindSize())
        .provideValues(std::move(contentSize), std::move(f))
        .bindWidgetMap([](auto drawContext, auto viewSize, auto contentSize,
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
                .map(clip())
                ;
        })
        ;
}
} // namespace reactive::widget
