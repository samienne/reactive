#include "pureconstraint.h"

#include "bqui/modifier/buildermodifier.h"

#include "widget/constraintlayout.h"

#include "bqui/widget/builder.h"

#include "bqui/buildparams.h"
#include "bqui/sizehint.h"

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <optional>
#include <utility>

namespace bqui::modifier::detail
{

namespace
{
    Axis toAxis(PureAxis axis)
    {
        return axis == PureAxis::horizontal ? Axis::x : Axis::y;
    }

    // The layout axis the enclosing pure-solver container seeded, as a signal so
    // its value tracks the real context the band is built in rather than a
    // parallel one that can diverge. A fill() widget flexes on the same axis a
    // filler() child of that container does.
    bq::signal::AnySignal<std::optional<Axis>> flexAxis(
            BuildParams const& params)
    {
        return params.valueOrDefault<widget::FlexAxisTag>();
    }

    // The shared shell of every pure-solver band modifier: @p set applied to the
    // builder.
    template <typename Set>
    AnyWidgetModifier pureBuilderModifier(Set set)
    {
        return makeWidgetModifier(makeBuilderModifier(
                [set = std::move(set)](widget::AnyBuilder builder)
                    -> widget::AnyBuilder
                {
                    set(builder);
                    return builder;
                }));
    }
} // namespace

AnyWidgetModifier pureNaturalModifier(PureAxis axis,
        arrange::Strength strength, bq::signal::AnySignal<float> value)
{
    return pureBuilderModifier(
            [axis, strength, value = std::move(value)](
                    widget::AnyBuilder& builder)
            {
                widget::setPureNatural(builder, toAxis(axis), value.clone(),
                        strength);
            });
}

AnyWidgetModifier pureFixedModifier(PureAxis axis,
        bq::signal::AnySignal<float> value)
{
    return pureBuilderModifier(
            [axis, value = std::move(value)](widget::AnyBuilder& builder)
            {
                widget::setPureFixed(builder, toAxis(axis), value.clone());
            });
}

AnyWidgetModifier pureMinModifier(PureAxis axis,
        bq::signal::AnySignal<float> value)
{
    return pureBuilderModifier(
            [axis, value = std::move(value)](widget::AnyBuilder& builder)
            {
                widget::setPureMin(builder, toAxis(axis), value.clone());
            });
}

AnyWidgetModifier pureMaxModifier(PureAxis axis,
        bq::signal::AnySignal<float> value)
{
    return pureBuilderModifier(
            [axis, value = std::move(value)](widget::AnyBuilder& builder)
            {
                widget::setPureMax(builder, toAxis(axis), value.clone());
            });
}

AnyWidgetModifier pureInsetModifier(bq::signal::AnySignal<float> amount)
{
    return pureBuilderModifier(
            [amount = std::move(amount)](widget::AnyBuilder& builder)
            {
                widget::applyPureInset(builder, amount.clone());
            });
}

AnyWidgetModifier pureFillModifier(float weight)
{
    return pureBuilderModifier(
            [weight](widget::AnyBuilder& builder)
            {
                // The flex lands on whichever axis the container stacks along, so
                // each axis's band is rebuilt with the seeded layout axis
                // threaded in as a signal.
                auto axisSig = flexAxis(builder.getBuildParams()).share();
                auto flexOn = [weight](widget::Constraints const& c,
                        Axis thisAxis, std::optional<Axis> layoutAxis)
                {
                    widget::Constraints out = c;
                    if (!layoutAxis || thisAxis == *layoutAxis)
                        out.flex = widget::Flex{ weight };
                    return out;
                };

                widget::PureLayout old = builder.getPureLayout();

                auto width = merge(old.getWidth(), axisSig.clone()).map(
                        [flexOn](widget::Constraints const& c,
                                std::optional<Axis> layoutAxis)
                        {
                            return flexOn(c, Axis::x, layoutAxis);
                        });

                builder.setPureLayout(widget::simplePureLayout(
                    bq::signal::AnySignal<widget::Constraints>(std::move(width)),
                    [old, flexOn, axisSig](
                            bq::signal::AnySignal<widget::LayoutSolution> ws)
                        -> bq::signal::AnySignal<widget::Constraints>
                    {
                        return merge(old.getHeightForWidth(std::move(ws)),
                                axisSig.clone()).map(
                                [flexOn](widget::Constraints const& c,
                                        std::optional<Axis> layoutAxis)
                                {
                                    return flexOn(c, Axis::y, layoutAxis);
                                });
                    }));
            });
}

AnyWidgetModifier pureGrowAxisModifier(PureAxis axis)
{
    return pureBuilderModifier(
            [axis](widget::AnyBuilder& builder)
            {
                widget::setPureFlex(builder, toAxis(axis),
                        bq::signal::constant(1.0f));
            });
}

AnyWidgetModifier composeModifiers(AnyWidgetModifier first,
        AnyWidgetModifier second)
{
    return makeWidgetModifier(
            [first = std::move(first), second = std::move(second)](
                widget::AnyWidget widget) -> widget::AnyWidget
            {
                return widget::AnyWidget(std::move(widget)
                            | AnyWidgetModifier(first))
                    | AnyWidgetModifier(second);
            });
}

} // namespace bqui::modifier::detail
