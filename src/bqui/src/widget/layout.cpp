#include "bqui/widget/layout.h"

#include "bqui/widget/widget.h"

#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/transform.h"
#include "bqui/modifier/addwidgets.h"

#include "bqui/provider/providebuildparams.h"

namespace bqui::widget
{

namespace
{

auto layout(SizeHintMap sizeHintMap, ObbMap obbMap,
        std::vector<widget::AnyBuilder> builders)
{
    auto hints = btl::fmap(builders, [](auto const& builder)
            {
                return builder.getSizeHint();
            });

    auto hintsSignal = bq::signal::combine(std::move(hints)).share();

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
            std::move(obbMap),
            hintsSignal,
            std::move(builders)
            );

    return std::move(widget)
        | modifier::setSizeHint(hintsSignal.map(std::move(sizeHintMap)))
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
            builders.push_back(std::move(widget)(params));

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

