#pragma once

#include "transform.h"
#include "instance.h"
#include "instancemodifier.h"
#include "builder.h"

#include "reactive/growsizehint.h"

#include "reactive/signal/signal.h"

#include <avg/transform.h>

#include <btl/fn.h>

namespace reactive::widget
{
    template <typename TSignalAmount>
    auto growSize(TSignalAmount amount)
    {
        return makeInstanceModifier([](Instance instance, auto amount)
            {
                auto size = instance.getObb().getSize();
                auto newSize = avg::Vector2f(
                    size[0] + 2.0f * amount,
                    size[1] + 2.0f * amount
                    );

                return std::move(instance)
                    .setObb(avg::Obb(newSize))
                    ;
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

        auto builderGrowSizeHint = [a](Builder builder)
            -> Builder
        {
            auto hint = signal::map(BTL_FN(growSizeHint),
                    builder.getSizeHint(),
                    a);

            return std::move(builder)
                .setSizeHint(std::move(hint));
        };

        return preMapBuilder(growSize(std::move(aNeg)))
            >> makeBuilderModifier(transform(std::move(t)))
            >> makeBuilderModifier(growSize(a))
            >> std::move(builderGrowSizeHint)
            ;
    }
} // reactive::widget

