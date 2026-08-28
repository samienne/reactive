#include "pureconstraint.h"

#include "bqui/modifier/buildermodifier.h"

#include "widget/constraintlayout.h"

#include "bqui/widget/builder.h"

#include "bqui/buildparams.h"
#include "bqui/sizehint.h"

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <utility>

namespace bqui::modifier::detail
{

namespace
{
    // The region-membership param is a constant the region owner seeded, so
    // evaluating it in a throwaway context is safe -- a constant does not
    // diverge between contexts.
    bool inPureRegion(BuildParams const& params)
    {
        auto context = bq::signal::makeSignalContext(
                params.valueOrDefault<widget::PureSolverTag>());
        return context.evaluate<0>().get<0>();
    }

    Axis toAxis(PureAxis axis)
    {
        return axis == PureAxis::horizontal ? Axis::x : Axis::y;
    }

    // The shared shell of every pure-solver band modifier: a no-op outside a
    // pure-solver region, and @p set applied to the builder inside one.
    template <typename Set>
    AnyWidgetModifier pureBuilderModifier(Set set)
    {
        return makeWidgetModifier(makeBuilderModifier(
                [set = std::move(set)](widget::AnyBuilder builder)
                    -> widget::AnyBuilder
                {
                    if (!inPureRegion(builder.getBuildParams()))
                        return builder;

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
