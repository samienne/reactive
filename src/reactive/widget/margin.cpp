#include "widget/margin.h"

#include "widget/transform.h"
#include "widget/instance.h"
#include "widget/instancemodifier.h"
#include "widget/widget.h"

#include <reactive/growsizehint.h>

#include <bq/signal/signal.h>
#include <bq/signal/merge.h>

#include <avg/transform.h>

#include <btl/fn.h>

namespace reactive::widget
{

namespace
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
} // anonymous namespace

AnyWidgetModifier margin(signal::AnySignal<float> amount)
{
    return makeWidgetModifier([](auto widget, auto amount)
    {
        auto aNeg = amount.map([](float f)
                {
                    return -f;
                });

        auto t = amount.map([](float amount)
                {
                    return avg::translate(amount, amount);
                });

        auto builderGrowSizeHint = makeBuilderModifier([](auto builder, auto amount)
                {
                    auto hint = merge(builder.getSizeHint(), amount)
                        .map(BTL_FN(growSizeHint));

                    return std::move(builder)
                        .setSizeHint(std::move(hint));
                },
                amount);

        return std::move(widget)
            | makeBuilderPreModifier(growSize(std::move(aNeg)))
            | transform(std::move(t))
            | growSize(amount)
            | std::move(builderGrowSizeHint)
            ;
    },
    std::move(amount).share()
    );
}

AnyWidgetModifier margin(float amount)
{
    return margin(signal::constant(amount));
}

} // namespace reactive::widget

