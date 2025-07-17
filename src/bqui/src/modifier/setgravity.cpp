#include "bqui/modifier/setgravity.h"

namespace bqui::modifier
{
    AnyWidgetModifier setGravity(bq::signal::AnySignal<avg::Vector2f> gravity)
    {
        return makeWidgetModifier(makeBuilderModifier([](auto builder, auto gravity)
            {
                return builder
                    .setGravity(gravity);
            },
            std::move(gravity)
            ));
    }

    AnyWidgetModifier setGravity(avg::Vector2f gravity)
    {
        return setGravity(bq::signal::constant(gravity));
    }
}
