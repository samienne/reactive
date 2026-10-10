#include "bqui/modifier/margin.h"

#include "pureconstraint.h"

#include "bqui/modifier/transform.h"
#include "bqui/modifier/buildermodifier.h"
#include "bqui/modifier/instancemodifier.h"

#include "bqui/widget/instance.h"
#include "bqui/widget/builder.h"
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

    // Builds the wrapped widget at the size shrunk by the inset on every side,
    // threading the region solution through so a pure container under a margin
    // still reads it to place its own children.
    widget::AnyBuilder shrinkBuilder(widget::AnyBuilder builder,
            bq::signal::AnySignal<float> amount)
    {
        auto sizeHint = builder.getSizeHint();
        auto gravity = builder.getGravity();
        auto params = builder.getBuildParams();
        auto box = builder.getBoxVariables();
        auto pureLayout = builder.getPureLayout();

        auto result = widget::makeBuilder(
            [builder = std::move(builder), amount = std::move(amount)](
                    BuildParams const&,
                    bq::signal::AnySignal<avg::Vector2f> size,
                    bq::signal::AnySignal<widget::LayoutSolution> solution)
                    -> widget::AnyElement
            {
                auto adjustedSize = merge(std::move(size), amount.clone()).map(
                        [](avg::Vector2f size, float amount) -> avg::Vector2f
                        {
                            return {
                                std::max(0.0f, size.x() - 2.0f * amount),
                                std::max(0.0f, size.y() - 2.0f * amount)
                            };
                        });

                return builder.clone()(std::move(adjustedSize),
                        std::move(solution));
            },
            std::move(sizeHint),
            std::move(params),
            std::move(gravity)
            );

        result.setBoxVariables(std::move(box));
        result.setPureLayout(std::move(pureLayout));
        return result;
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

        auto shrinkModifier = makeBuilderModifier(
                [](widget::AnyBuilder builder, auto amount)
                {
                    return shrinkBuilder(std::move(builder), std::move(amount));
                },
                amount);

        // pureInsetModifier wraps the descriptor's band in a fresh outer box
        // grown by the inset; the shrink/translate/grow above places the inset
        // content at build time.
        return std::move(widget)
            | shrinkModifier
            | transform(std::move(t))
            | growSize(amount)
            | std::move(builderGrowSizeHint)
            | detail::pureInsetModifier(amount)
            ;
    },
    std::move(amount).share()
    );
}

}

