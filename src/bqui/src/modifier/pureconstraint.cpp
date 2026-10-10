#include "pureconstraint.h"

#include "bqui/modifier/buildermodifier.h"

#include "widget/constraintlayout.h"

#include "bqui/widget/builder.h"


#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <utility>

namespace bqui::modifier::detail
{

namespace
{
    Axis toAxis(PureAxis axis)
    {
        return axis == PureAxis::horizontal ? Axis::x : Axis::y;
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

AnyWidgetModifier pureAnchorModifier(PureAxis axis, std::string name,
        bq::signal::AnySignal<widget::Anchor> anchor)
{
    return pureBuilderModifier(
            [axis, name = std::move(name), anchor = std::move(anchor)](
                    widget::AnyBuilder& builder)
            {
                widget::setPureAnchor(builder, toAxis(axis), name,
                        anchor.clone());
            });
}

AnyWidgetModifier pureFillModifier(float weight)
{
    return pureBuilderModifier(
            [weight](widget::AnyBuilder& builder)
            {
                widget::setPureFlex(builder, Axis::x,
                        bq::signal::constant(weight));
                widget::setPureFlex(builder, Axis::y,
                        bq::signal::constant(weight));
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
