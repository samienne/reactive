#include "pureconstraint.h"

#include "bqui/modifier/buildermodifier.h"

#include "widget/constraintlayout.h"

#include "bqui/widget/builder.h"

#include "bqui/buildparams.h"
#include "bqui/sizehint.h"

#include <bq/signal/constant.h>
#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <arrange/expression.h>
#include <arrange/variable.h>

#include <optional>
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

    // The layout axis and shared flex variable the enclosing pure-solver
    // container seeded for its fillers, as signals. They stay signals so their
    // values track the real context the band is built in rather than a parallel
    // one that can diverge. A fill() widget couples to the same pair a filler()
    // child of that container does.
    bq::signal::AnySignal<Axis> flexAxis(BuildParams const& params)
    {
        return params.valueOrDefault<widget::FlexAxisTag>();
    }

    bq::signal::AnySignal<arrange::Variable> flexVariable(
            BuildParams const& params)
    {
        return params.valueOrDefault<widget::FlexVariableTag>();
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

AnyWidgetModifier pureFillModifier(float weight)
{
    return pureBuilderModifier(
            [weight](widget::AnyBuilder& builder)
            {
                BuildParams const& params = builder.getBuildParams();
                auto axisSig = flexAxis(params).share();
                auto flexSig = flexVariable(params).share();
                widget::BoxVariables box = builder.getBoxVariables();

                // The same coupling a filler emits, weighted: on the container's
                // layout axis the widget's extent equals weight times the shared
                // flex variable, at the weakest tier, so the slack splits between
                // every filler and fill() sibling in proportion to their weights.
                // The content natural rides up as a flex-basis and is dropped when
                // the container stamps it (flattenConstraints). Composed onto the
                // existing band per axis, so the layout axis and flex variable
                // thread in as the seeded signals.
                auto couple = [box, weight](widget::Constraints const& c,
                        Axis thisAxis, Axis layoutAxis, arrange::Variable flex)
                {
                    if (thisAxis != layoutAxis)
                        return c;

                    widget::Constraints out = c;
                    out.flex = widget::Flex{ weight };
                    out.relations.constraints.push_back(
                            ((thisAxis == Axis::x ? box.width() : box.height())
                                == static_cast<double>(weight)
                                    * arrange::Expression(flex))
                            | widget::weakestStrength());
                    return out;
                };

                std::optional<widget::PureLayout> current =
                    builder.getPureLayout();
                widget::PureLayout old = current ? *current
                    : widget::simplePureLayout(
                        bq::signal::constant(widget::Constraints()),
                        [](bq::signal::AnySignal<widget::LayoutSolution>)
                        {
                            return bq::signal::AnySignal<widget::Constraints>(
                                    bq::signal::constant(widget::Constraints()));
                        });

                auto width = merge(old.getWidth(), axisSig.clone(),
                        flexSig.clone()).map(
                        [couple](widget::Constraints const& c, Axis layoutAxis,
                                arrange::Variable flex)
                        {
                            return couple(c, Axis::x, layoutAxis, flex);
                        });

                builder.setPureLayout(widget::simplePureLayout(
                    bq::signal::AnySignal<widget::Constraints>(std::move(width)),
                    [old, couple, axisSig, flexSig](
                            bq::signal::AnySignal<widget::LayoutSolution> ws)
                        -> bq::signal::AnySignal<widget::Constraints>
                    {
                        return merge(old.getHeightForWidth(std::move(ws)),
                                axisSig.clone(), flexSig.clone()).map(
                                [couple](widget::Constraints const& c,
                                        Axis layoutAxis, arrange::Variable flex)
                                {
                                    return couple(c, Axis::y, layoutAxis, flex);
                                });
                    }));
            });
}

AnyWidgetModifier pureContentDefaultModifier()
{
    return pureBuilderModifier(
            [](widget::AnyBuilder& builder)
            {
                builder.setPureLayout(widget::pureLayoutFromSizeHint(
                        builder.getSizeHint(), builder.getBoxVariables()));
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
