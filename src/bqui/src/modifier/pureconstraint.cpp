#include "pureconstraint.h"

#include "bqui/modifier/buildermodifier.h"

#include "widget/constraintlayout.h"

#include "bqui/widget/builder.h"

#include "bqui/buildparams.h"

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <memory>
#include <utility>

namespace bqui::modifier::detail
{

namespace
{
    using widget::BoxVariables;
    using widget::LayoutSpec;

    using CollectorHandle = std::weak_ptr<bq::signal::SharedVector<
        bq::signal::AnySignal<LayoutSpec>>>;

    // The region-membership params are constants the region owner seeded, so
    // evaluating them in a throwaway context is safe -- a constant does not
    // diverge between contexts.
    bool inPureRegion(BuildParams const& params)
    {
        auto context = bq::signal::makeSignalContext(
                params.valueOrDefault<widget::PureSolverTag>());
        return context.evaluate<0>().get<0>();
    }

    CollectorHandle collectorFor(BuildParams const& params, PureAxis axis)
    {
        auto entry = axis == PureAxis::horizontal
            ? params.valueOrDefault<widget::RegionCollectorTag>()
            : params.valueOrDefault<widget::RegionVerticalCollectorTag>();

        auto context = bq::signal::makeSignalContext(std::move(entry));
        return context.evaluate<0>().get<0>();
    }
} // namespace

AnyWidgetModifier pureConstraintModifier(PureAxis axis,
        bq::signal::AnySignal<float> value,
        btl::Function<std::vector<arrange::Constraint>(
            BoxVariables const&, float)> make)
{
    return makeWidgetModifier(makeBuilderModifier(
            [axis, value = std::move(value), make = std::move(make)](
                widget::AnyBuilder builder) -> widget::AnyBuilder
            {
                BuildParams const& params = builder.getBuildParams();
                if (!inPureRegion(params))
                    return builder;

                BoxVariables box = builder.getBoxVariables();

                auto fragment = value.clone().map(
                        [box, make](float v)
                        {
                            LayoutSpec spec;
                            spec.constraints = make(box, v);
                            return spec;
                        });

                if (auto collector = collectorFor(params, axis).lock())
                    collector->write()->push_back(
                            bq::signal::AnySignal<LayoutSpec>(
                                std::move(fragment)));

                return builder;
            }));
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
