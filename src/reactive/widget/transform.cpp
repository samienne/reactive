#include "widget/transform.h"

#include "widget/instancemodifier.h"

#include <reactive/signal2/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{

AnyInstanceModifier transformBuilder(signal2::AnySignal<avg::Transform> t)
{
    return makeInstanceModifier([](Instance instance, avg::Transform const& t)
            {
                return std::move(instance).transform(t);
            },
            std::move(t)
            );
}

AnyWidgetModifier transform(signal2::AnySignal<avg::Transform> t)
{
    return makeWidgetModifier(transformBuilder(std::move(t)));
}

} // namespace reactive::widget

