#include "widget/bin.h"

#include "widget/binddrawcontext.h"
#include "widget/clip.h"

#include "bindwidgetmap.h"

namespace reactive::widget
{

WidgetMap bin(WidgetFactory f)
{
    auto sizeHint = signal::share(f.getSizeHint());

    return makeWidgetMap()
        .provide(bindDrawContext(), bindSize())
        .provideValues(std::move(f))
        .bindWidgetMap([](auto drawContext, auto viewSize, auto f) mutable
        {
            auto contentSize = signal::share(signal::map([](auto hint)
                    {
                        float w = hint.getWidth()[1];
                        float h = hint.getHeightForWidth(w)[1];

                        return avg::Vector2f(w, h);
                    },
                    f.getSizeHint()
                    ));

            auto t = signal::map([](avg::Vector2f viewSize,
                        avg::Vector2f contentSize)
                    {
                        float offY = contentSize[1] - viewSize[1];
                        return avg::translate(0.0f, -offY);
                    },
                    std::move(viewSize),
                    contentSize
                    );

            auto w = std::move(f)(
                    std::move(drawContext),
                    std::move(contentSize)
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
