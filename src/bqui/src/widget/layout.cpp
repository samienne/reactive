#include "bqui/widget/layout.h"

#include "bqui/widget/widget.h"

#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/handlegravity.h"
#include "bqui/modifier/setwidgetintrospection.h"
#include "bqui/modifier/setid.h"

#include "bqui/provider/providebuildparams.h"

#include <bq/signal/arraysignal.h>

namespace bqui::widget
{

namespace
{

// Each child gets its own id so the render tree matches children by identity,
// not by position, when the list changes.
bq::signal::AnySignal<widget::Instance> buildChild(
        widget::AnyBuilder const& builder,
        bq::signal::AnySignal<avg::Obb> obb)
{
    auto shared = obb.share();
    auto transform = shared.map(&avg::Obb::getTransform);
    auto size = shared.map(&avg::Obb::getSize);

    auto placed = builder.clone()
        | modifier::transformBuilder(std::move(transform));

    return (std::move(placed)(std::move(size))
        | modifier::setElementId(bq::signal::constant(avg::UniqueId()))
        ).getInstance();
}

auto layout(SizeHintMap sizeHintMap, ObbMap obbMap,
        bq::signal::ArraySignal<widget::AnyBuilder> array)
{
    auto hints = bq::signal::join(array.map(
                [](widget::AnyBuilder const& builder)
                {
                    return builder.getSizeHint();
                })).share();

    auto widget = makeWidgetWithSize(
            [](auto size, auto obbMap, auto hints, auto array)
            {
                auto obbs = merge(std::move(size), std::move(hints))
                    .map(std::move(obbMap));

                auto instances = bq::signal::join(bq::signal::scatter(
                            std::move(array), std::move(obbs), &buildChild));

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(instances))
                    | modifier::setRole("Layout")
                    ;
            }
            ,
            std::move(obbMap),
            hints,
            std::move(array)
            );

    return std::move(widget)
        | modifier::setSizeHint(hints.map(std::move(sizeHintMap)))
        ;
}

} // anonymous namespace

widget::AnyWidget layout(SizeHintMap sizeHintMap,
        ObbMap obbMap, bq::signal::ArraySignal<widget::AnyWidget> widgets)
{
    return makeWidget([](BuildParams const& params,
                SizeHintMap sizeHintMap, ObbMap obbMap, auto widgets)
    {
        auto builders = widgets.map(
                [params](widget::AnyWidget const& widget)
                -> widget::AnyBuilder
                {
                    return (widget.clone()
                            | modifier::handleGravity()
                            )(params);
                });

        return layout(
                std::move(sizeHintMap),
                std::move(obbMap),
                std::move(builders)
                );
    },
    provider::provideBuildParams(),
    std::move(sizeHintMap),
    std::move(obbMap),
    std::move(widgets)
    );
}
}
