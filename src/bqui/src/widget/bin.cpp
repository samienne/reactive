#include "bqui/widget/bin.h"

#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/clip.h"
#include "bqui/modifier/transform.h"

namespace bqui::widget
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
                | modifier::transform(std::move(offset))
                ;

            return makeWidget()
                | modifier::addWidget(std::move(transformedContent)
                        (params)
                        (std::move(contentSize)).getInstance()
                        )
                | modifier::clip()
                ;
        },
        provider::provideBuildParams(),
        contentSize.share(),
        std::move(contentWidget)
        );
}

}

