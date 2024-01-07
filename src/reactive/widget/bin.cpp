#include "widget/bin.h"

#include "widget/addwidgets.h"
#include "widget/clip.h"
#include "widget/transform.h"
#include "widget/instancemodifier.h"
#include "widget/buildermodifier.h"

namespace reactive::widget
{

AnyWidgetModifier bin(AnyBuilder builder, AnySignal<avg::Vector2f> contentSize)
{
    return makeWidgetModifier([](auto widget, auto contentSize, auto builder)
        {
            return std::move(widget)
            | makeSharedInstanceSignalModifier([](auto widget, auto contentSize, auto builder)
            {
                auto viewSize = signal::map(&Instance::getSize, widget);

                auto t = signal::map([](avg::Vector2f viewSize,
                            avg::Vector2f contentSize)
                        {
                            float offY = contentSize[1] - viewSize[1];
                            return avg::translate(0.0f, -offY);
                        },
                        std::move(viewSize),
                        contentSize
                        );

                auto newBuilder = std::move(builder)
                        | transform(std::move(t))
                        ;

                auto newInstance = std::move(newBuilder)(std::move(contentSize))
                    .getInstance();

                return std::move(widget)
                    | addWidget(std::move(newInstance))
                    ;
            },
            share(std::move(contentSize)),
            std::move(builder)
            )
            | clip()
            ;
        },
        share(std::move(contentSize)),
        std::move(builder)
        );

}

} // namespace reactive::widget

