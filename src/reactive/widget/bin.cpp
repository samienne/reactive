#include "widget/bin.h"

#include "widget/addwidgets.h"
#include "widget/clip.h"
#include "widget/transform.h"
#include "widget/instancemodifier.h"
#include "widget/buildermodifier.h"

namespace reactive::widget
{

AnyWidget bin(AnyWidget contentWidget, AnySignal<avg::Vector2f> contentSize)
{
    return makeWidgetWithSize(
        [](auto viewSize, BuildParams const& params, auto contentSize, auto contentWidget)
        {
            auto offset = signal::map([](avg::Vector2f viewSize,
                        avg::Vector2f contentSize)
                    {
                        float offY = contentSize[1] - viewSize[1];
                        return avg::translate(0.0f, -offY);
                    },
                    std::move(viewSize),
                    contentSize
                    );

            auto transformedContent = std::move(contentWidget)
                | transform(std::move(offset))
                ;

            return makeWidget()
                | addWidget(std::move(transformedContent)
                        (params)
                        (std::move(contentSize)).getInstance()
                        )
                | clip()
                ;
        },
        provideBuildParams(),
        signal::share(std::move(contentSize)),
        std::move(contentWidget)
        );
}

} // namespace reactive::widget

