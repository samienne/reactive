#include "widget/bin.h"

#include "widget/addwidgets.h"
#include "widget/clip.h"
#include "widget/transform.h"

namespace reactive::widget
{

AnyWidget bin(AnyWidget contentWidget,
        bq::signal::AnySignal<avg::Vector2f> contentSize)
{
    return makeWidgetWithSize(
        [](auto viewSize, BuildParams const& params, auto contentSize, auto contentWidget)
        {
            auto offset = merge(viewSize, contentSize).map(
                    [](avg::Vector2f viewSize, avg::Vector2f contentSize)
                    {
                        float offY = contentSize[1] - viewSize[1];
                        return avg::translate(0.0f, -offY);
                    });

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
        contentSize.share(),
        std::move(contentWidget)
        );
}

} // namespace reactive::widget

