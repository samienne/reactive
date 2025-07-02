#include "bqui/modifier/background.h"

#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/setgravity.h"

#include "bqui/provider/providetheme.h"

#include "bqui/shape/rectangle.h"

#include <bq/signal/signal.h>

#include <avg/brush.h>

namespace bqui::modifier
{
    AnyWidgetModifier background(bq::signal::AnySignal<avg::Brush> brush)
    {
        return background(shape::rectangle().fill(std::move(brush)));
    }

    AnyWidgetModifier background()
    {
        return makeWidgetModifier([](auto widget, auto theme)
            {
                auto bg = std::move(theme).map([](Theme const& theme)
                    {
                        return avg::Brush(theme.getBackground());
                    });

                return std::move(widget)
                    | background(std::move(bg))
                    ;
            },
            provider::provideTheme()
            );
    }

    AnyWidgetModifier background(widget::AnyWidget bgWidget)
    {
        return makeWidgetModifier([](auto widget, auto bgWidget,
                    BuildParams const& params)
        {
            auto builder = std::move(widget)(params);
            auto sizeHint = builder.getSizeHint();
            auto gravity = builder.getGravity();

            return makeWidgetWithSize([](auto size, BuildParams const& params,
                        auto builder, auto bgWidget)
            {
                auto s = std::move(size).share();
                auto fgElement = std::move(builder)(s);
                auto bgElement = std::move(bgWidget)(params)(s);

                auto newInstance = merge(
                        std::move(fgElement).getInstance(),
                        std::move(bgElement).getInstance()).map(
                        [](auto fgInstance, auto bgInstance)
                        {
                            return addWidgets(std::move(bgInstance),
                                    { std::move(fgInstance) });
                        });

                return makeWidgetFromElement(
                        makeElement(std::move(newInstance), params)
                        )
                    ;
            },
            params,
            std::move(builder),
            std::move(bgWidget)
            )
            | setGravity(std::move(gravity))
            | setSizeHint(std::move(sizeHint))
            ;
        },
        std::move(bgWidget),
        provider::provideBuildParams()
        );
    }
}

