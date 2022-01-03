#include "widget/bin.h"

#include "widget/addwidgets.h"
#include "widget/clip.h"
#include "widget/transform.h"
#include "widget/widgetmodifier.h"

namespace reactive::widget
{

WidgetTransformer<void> bin(WidgetFactory f, AnySignal<avg::Vector2f> contentSize)
{
    return makeSharedWidgetSignalModifier([](auto widget, auto contentSize, auto f)
        {
            auto viewSize = signal::map(&Widget::getSize, widget);

            auto t = signal::map([](avg::Vector2f viewSize,
                        avg::Vector2f contentSize)
                    {
                        float offY = contentSize[1] - viewSize[1];
                        return avg::translate(0.0f, -offY);
                    },
                    std::move(viewSize),
                    contentSize
                    );

            auto newWidget = std::move(f)
                    .map(transform(std::move(t)))
                    (
                    std::move(contentSize)
                    )
                    ;

            return std::move(widget)
                | addWidget(std::move(newWidget))
                | clip()
                ;
        },
        share(std::move(contentSize)),
        std::move(f)
        );
}

} // namespace reactive::widget

