#include "widget/setsizehint.h"

#include "widget/buildermodifier.h"
#include "widget/widget.h"

namespace reactive::widget
{

AnyWidgetModifier setSizeHint(signal2::AnySignal<SizeHint>sizeHint)
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

} // namespace reactive::widget

