#include "bqui/modifier/transform.h"

#include "bqui/modifier/instancemodifier.h"

#include <bq/signal/signal.h>

#include <avg/transform.h>

namespace bqui::modifier
{

AnyInstanceModifier transformBuilder(bq::signal::AnySignal<avg::Transform> t)
{
    return makeInstanceModifier([](widget::Instance instance,
                avg::Transform const& t)
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

}

