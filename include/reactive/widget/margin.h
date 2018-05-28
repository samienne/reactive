#pragma once

#include "reactive/widget.h"
#include "reactive/widgetfactory.h"

#include "reactive/signal.h"

#include <avg/transform.h>

#include <btl/fn.h>

#include <fit/compose.h>

#include <iostream>

namespace reactive
{
    namespace widget
    {
        template <typename TSignalAmount>
        auto growSize(TSignalAmount amount)
        {
            return makeWidgetMap<ObbTag>(
                [](avg::Obb const& obb, float amount) -> avg::Obb
                {
                    auto size = obb.getSize();
                    auto newSize = avg::Vector2f(
                        size[0] + 2.0f * amount,
                        size[1] + 2.0f * amount
                        );

                    auto translate = avg::Transform()
                        .translate(-amount, -amount);

                    return obb.getTransform() * translate * avg::Obb(newSize);
                },
                std::move(amount)
                );
        }

        template <typename TSignalAmount, typename = std::enable_if_t<
            IsSignalType<TSignalAmount, float>::value
            >
        >
        auto margin(TSignalAmount amount)
        {
            auto a = signal::share(std::move(amount));
            auto aNeg = signal::map([](float f)
                    {
                        return -f;
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
                >> mapFactoryWidget(growSize(a))
                >> std::move(factoryGrowSizeHint)
                ;
        }
    } // widget
} // reactive

