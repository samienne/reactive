#include "widget/transform.h"

#include "widget/instancemodifier.h"

#include <bq/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{

AnyInstanceModifier transformBuilder(bq::signal::AnySignal<avg::Transform> t)
{
    return makeInstanceModifier([](Instance instance, avg::Transform const& t)
            {
                return std::move(instance).transform(t);
            },
            std::move(t)
            );
}

AnyWidgetModifier transform(bq::signal::AnySignal<avg::Transform> t)
{
    return makeWidgetModifier(transformBuilder(std::move(t)));
}

} // namespace reactive::widget

