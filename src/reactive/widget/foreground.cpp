#include "reactive/widget/foreground.h"

#include "reactive/widget/addwidgets.h"
#include "reactive/widget/setsizehint.h"

#include <avg/brush.h>

namespace reactive::widget
{
    AnyWidgetModifier foreground(AnyWidget fgWidget)
    {
        return makeWidgetModifier([](auto widget, auto fgWidget,
                    BuildParams const& params)
        {
            auto builder = std::move(widget)(params);
            auto sizeHint = builder.getSizeHint();

            return makeWidgetWithSize([](auto size, BuildParams const& params,
                        auto builder, auto fgWidget)
            {
                auto s = signal::share(std::move(size));
                auto bgElement = std::move(builder)(s);
                auto fgElement = std::move(fgWidget)(params)(s);

                auto newInstance = signal::group(
                        std::move(fgElement).getInstance(),
                        std::move(bgElement).getInstance()).map(
                        [](auto fgInstance, auto bgInstance)
                        {
                            return addWidgets(std::move(bgInstance),
                                    { std::move(fgInstance) });
                        });

                return makeWidgetFromElement(
                        makeElement(std::move(newInstance), params));
            },
            params,
            std::move(builder),
            std::move(fgWidget)
            )
            | setSizeHint(std::move(sizeHint))
            ;
        },
        std::move(fgWidget),
        provideBuildParams()
        );
    }
} // namespace reactive::widget

