#pragma once

#include "setobb.h"
#include "bindobb.h"
#include "transform.h"
#include "widgettransformer.h"

#include "reactive/widget.h"
#include "reactive/widgetfactory.h"
#include "reactive/growsizehint.h"

#include "reactive/signal/signal.h"

#include <avg/transform.h>

#include <btl/fn.h>

namespace reactive::widget
{
    template <typename TSignalAmount>
    auto growSize(TSignalAmount amount)
    {
        /*
        return makeWidgetTransformer()
            .compose(grabObb())
            .values(std::move(amount))
            .bind([](auto obb, auto amount)
            {
                auto newObb = signal::map(
                    [](avg::Obb const& obb, float amount) -> avg::Obb
                    {
                        auto size = obb.getSize();
                        auto newSize = avg::Vector2f(
                            size[0] + 2.0f * amount,
                            size[1] + 2.0f * amount
                            );

                        return avg::Obb(newSize);
                    },
                    std::move(obb),
                    std::move(amount)
                    );

                return setObb(std::move(newObb));
            });
            */
        return makeWidgetModifier([](auto widget, auto amount)
            {
                return signal::map([](Widget widget, float amount)
                    {
                        auto size = widget.getObb().getSize();
                        auto newSize = avg::Vector2f(
                            size[0] + 2.0f * amount,
                            size[1] + 2.0f * amount
                            );

                        return std::move(widget)
                            .setObb(avg::Obb(newSize))
                            ;
                    },
                    std::move(widget),
                    std::move(amount)
                    );
            },
            std::forward<TSignalAmount>(amount)
            );
    }

    template <typename T>
    auto margin(Signal<T, float> amount)
    {
        auto a = signal::share(std::move(amount));
        auto aNeg = signal::map([](float f)
                {
                    return -f;
                }, a);

        auto t = signal::map([](float amount)
                {
                    return avg::translate(amount, amount);
                }, a);

        auto factoryGrowSizeHint = [a](WidgetFactory factory)
            -> WidgetFactory
        {
            auto hint = signal::map(BTL_FN(growSizeHint),
                    factory.getSizeHint(),
                    a);

            return std::move(factory)
                .setSizeHint(std::move(hint));
        };

        return preMapFactory(growSize(std::move(aNeg)))
            >> mapFactoryWidget(transform(std::move(t)))
            >> mapFactoryWidget(growSize(a))
            >> std::move(factoryGrowSizeHint)
            ;
    }
} // reactive::widget

