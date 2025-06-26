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

}

