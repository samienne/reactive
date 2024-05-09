#include "widget/transform.h"

#include "widget/instancemodifier.h"

#include <reactive/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{

AnyInstanceModifier transformBuilder(signal::AnySignal<avg::Transform> t)
{
    return makeInstanceModifier([](Instance instance, avg::Transform const& t)
            {
                return std::move(instance).transform(t);
            },
            std::move(t)
            );
}

AnyWidgetModifier transform(signal::AnySignal<avg::Transform> t)
{
    return makeWidgetModifier(transformBuilder(std::move(t)));
}

} // namespace reactive::widget

