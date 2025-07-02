#include "bqui/modifier/setsizehint.h"

#include "bqui/modifier/buildermodifier.h"

namespace bqui::modifier
{

AnyWidgetModifier setSizeHint(bq::signal::AnySignal<SizeHint>sizeHint)
{
    return makeWidgetModifier(makeBuilderModifier([](auto builder, auto sizeHint)
    {
        return std::move(builder)
            .setSizeHint(std::move(sizeHint))
            ;
    },
    std::move(sizeHint)
    ));
}

AnyWidgetModifier setSizeHint(SizeHint sizeHint)
{
    return setSizeHint(bq::signal::constant(std::move(sizeHint)));
}

AnyWidgetModifier setSizeHint(avg::Vector2f requestedSize)
{
    return setSizeHint(bq::signal::constant(
                simpleSizeHint(requestedSize.x(), requestedSize.y())
                ));
}

}

