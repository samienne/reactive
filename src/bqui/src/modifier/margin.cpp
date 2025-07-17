#include "bqui/modifier/margin.h"

#include "bqui/modifier/transform.h"
#include "bqui/modifier/instancemodifier.h"

#include "bqui/widget/instance.h"
#include "bqui/widget/widget.h"

#include <bqui/growsizehint.h>

#include <bq/signal/signal.h>
#include <bq/signal/merge.h>

#include <avg/transform.h>

#include <btl/fn.h>

namespace bqui::modifier
{

namespace
{
    template <typename TSignalAmount>
    auto growSize(TSignalAmount amount)
    {
        return makeInstanceModifier([](widget::Instance instance, auto amount)
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

    template <typename T, typename U, typename V>
    auto marginWidgetModifier(T widget, U&& size, BuildParams const& params,
            bq::signal::Signal<V, float> amount)
    {
        auto builder = std::move(widget)(params);

        auto adjustedSize = merge(size, amount).map(
                [](avg::Vector2f size, float amount) -> avg::Vector2f
                {
                    return {
                        std::max(0.0f, size.x() - 2.0f * amount),
                        std::max(0.0f, size.y() - 2.0f * amount)
                    };
                });

        auto element = std::move(builder)(std::move(adjustedSize));

        return widget::makeWidgetFromElement(std::move(element));;
    }
} // anonymous namespace

AnyWidgetModifier margin(bq::signal::AnySignal<float> amount)
{
    return makeWidgetModifier([](auto widget, auto amount)
    {
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

        auto shrinkModifier = makeWidgetModifierWithSize(
                BTL_FN(marginWidgetModifier), provider::provideBuildParams(),
                amount);

        return std::move(widget)
            | shrinkModifier
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
    return margin(bq::signal::constant(amount));
}

}

