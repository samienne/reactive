#include "bqui/widget/layout.h"

#include "bqui/widget/widget.h"

#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/addwidgets.h"
#include "bqui/modifier/handlegravity.h"
#include "bqui/modifier/setwidgetintrospection.h"

#include "bqui/widget/introspection.h"

#include "bqui/provider/providebuildparams.h"

#include <bq/signal/combine.h>

#include <algorithm>

namespace bqui::widget
{

namespace
{

// The size a size hint asks for when unconstrained: its natural size (index 1
// of the min/natural/stretch triple) on each axis. Introspection geometry is
// resolved at this size, since the realised size is not known where
// introspection is aggregated (builder level).
avg::Vector2f naturalSize(SizeHint const& hint)
{
    auto width = hint.getWidth();
    auto height = hint.getHeightForWidth(width[1]);
    return avg::Vector2f(width[1], height[1]);
}

auto layout(SizeHintMap sizeHintMap, ObbMap obbMap,
        std::vector<widget::AnyBuilder> builders)
{
    auto hints = btl::fmap(builders, [](auto const& builder)
            {
                return builder.getSizeHint();
            });

    auto hintsSignal = bq::signal::combine(std::move(hints)).share();

    std::vector<bq::signal::AnySignal<Introspection>> childIntrospections;
    childIntrospections.reserve(builders.size());
    for (auto const& builder : builders)
        childIntrospections.push_back(builder.getIntrospection());

    auto widget = makeWidgetWithSize(
            [](auto size, auto obbMap, auto hintsSignal, auto builders)
            {
                auto obbs = merge(std::move(size), hintsSignal).map(obbMap).share();

                size_t index = 0;
                auto widgets = btl::fmap(builders, [&index, &obbs](auto&& f)
                        -> bq::signal::AnySignal<widget::Instance>
                    {
                        auto t = obbs.map([index](
                                    std::vector<avg::Obb> const& obbs)
                                {
                                    return obbs.at(index).getTransform();
                                });

                        auto size = obbs.map([index](
                                    std::vector<avg::Obb> const& obbs)
                                {
                                    return obbs.at(index).getSize();
                                });

                        auto builder = f.clone()
                            | modifier::transformBuilder(std::move(t));

                        ++index;

                        return std::move(builder)(std::move(size)).getInstance();
                    });

                return widget::makeWidget()
                    | modifier::addWidgets(std::move(widgets))
                    ;
            }
            ,
            obbMap,
            hintsSignal,
            std::move(builders)
            );

    // Aggregate child introspection at builder level (before realisation), so
    // it is observable through getIntrospection() without driving a size. Each
    // child's placement transform is derived by running the layout's obbMap at
    // the natural size, then lifted onto the child's subtree so obbs compose to
    // window space at the root.
    auto introspection = merge(
            bq::signal::combine(std::move(childIntrospections)),
            hintsSignal
            )
        .map([obbMap=std::move(obbMap)](std::vector<Introspection> children,
                std::vector<SizeHint> const& hints)
        {
            avg::Vector2f size(0.0f, 0.0f);
            for (auto const& hint : hints)
            {
                auto s = naturalSize(hint);
                size[0] = std::max(size[0], s[0]);
                size[1] = std::max(size[1], s[1]);
            }

            auto obbs = obbMap(size, hints);
            for (size_t i = 0; i < children.size() && i < obbs.size(); ++i)
                children[i] = transformIntrospection(std::move(children[i]),
                        obbs.at(i).getTransform());

            Introspection data;
            data.role = "Layout";
            data.obb = avg::Obb(size);
            data.children = std::move(children);
            return data;
        });

    return std::move(widget)
        | modifier::setSizeHint(hintsSignal.map(std::move(sizeHintMap)))
        | modifier::setWidgetIntrospection(std::move(introspection))
        ;
}

} // anonymous namespace

widget::AnyWidget layout(SizeHintMap sizeHintMap,
        ObbMap obbMap, std::vector<widget::AnyWidget> widgets)
{
    return makeWidget([](BuildParams const& params,
                SizeHintMap sizeHintMap, ObbMap obbMap, auto widgets)
    {
        std::vector<widget::AnyBuilder> builders;

        for (auto&& widget : widgets)
        {
            builders.push_back((
                        std::move(widget)
                        | modifier::handleGravity()
                        )(params));
        }

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
