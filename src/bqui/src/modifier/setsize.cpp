#include "bqui/modifier/setsize.h"

#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/buildermodifier.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/handlegravity.h"

using namespace bq::signal;

namespace bqui::modifier
{
    /*
    auto setSizeBuilderModifier(widget::AnyBuilder builder,
            AnySignal<avg::Vector2f> outerSize,
            AnySignal<avg::Vector2f> innerSize) // -> widget::AnyBuilder
    {
        auto offset = merge(innerSize, outerSize, builder.getGravity()).map(
                [](avg::Vector2f innerSize, avg::Vector2f outerSize,
                    avg::Vector2f gravity)
                -> avg::Transform
            {
                return avg::Transform().translate(avg::Vector2f {
                    gravity.x() * (outerSize.x() - innerSize.x()),
                    gravity.y() * (outerSize.y() - innerSize.y())
                    });
            });

        auto element = std::move(builder)(innerSize);

        return makeBuilderFromElement(std::move(element))
            | transformBuilder(offset)
            ;
    }
    */

    auto setSizeModifier(widget::AnyWidget widget,
            //AnySignal<avg::Vector2f> outerSize,
            AnySignal<avg::Vector2f> innerSize) // -> Widget
    {
        auto sizeHint = innerSize.map([](avg::Vector2f innerSize)
            {
                return simpleSizeHint(innerSize.x(), innerSize.y());
            });

        return std::move(widget)
            /*| makeBuilderModifier(setSizeBuilderModifier,
                    std::move(outerSize),
                    std::move(innerSize))*/
            | handleGravity()
            | setSizeHint(std::move(sizeHint));
    }

    AnyWidgetModifier setSize(AnySignal<avg::Vector2f> size)
    {
        return makeWidgetModifier(setSizeModifier, std::move(size));
    }

    AnyWidgetModifier setSize(avg::Vector2f size)
    {
        return setSize(bq::signal::constant(size));
    }
}
