#include "bqui/widget/bin.h"

#include "constraintbox.h"

#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/clip.h"
#include "bqui/modifier/transform.h"

#include "bqui/provider/providebuildparams.h"

namespace bqui::widget
{

AnyWidget bin(AnyWidget contentWidget,
        bq::signal::AnySignal<avg::Vector2f> contentSize)
{
    return makeWidgetWithSize(
        [](auto viewSize, BuildParams const& params, auto contentSize,
                auto contentWidget)
        {
            auto sharedContentSize = std::move(contentSize).share();

            auto offset = merge(viewSize, sharedContentSize.clone()).map(
                    [](avg::Vector2f viewSize, avg::Vector2f contentSize)
                    {
                        float offY = contentSize[1] - viewSize[1];
                        return avg::translate(0.0f, -offY);
                    });

            if (pureSolver(params))
            {
                // The firewall: the content is solved as its own pure region
                // anchored to its content size, so its containers place their
                // children, and its size dies here rather than crossing into the
                // parent's solve. The scroll offset and clip are applied to the
                // solved instance exactly as the transform modifier would.
                auto instance = solvePureRegionAtSize(contentWidget,
                        sharedContentSize.clone(), params);

                auto transformed = merge(std::move(instance), std::move(offset))
                    .map([](widget::Instance instance, avg::Transform t)
                        {
                            return std::move(instance).transform(t);
                        });

                return makeWidget()
                    | modifier::addWidget(std::move(transformed))
                    | modifier::clip()
                    ;
            }

            auto transformedContent = std::move(contentWidget)
                | modifier::transform(std::move(offset))
                ;

            return makeWidget()
                | modifier::addWidget(std::move(transformedContent)
                        (params)
                        (sharedContentSize.clone()).getInstance()
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

