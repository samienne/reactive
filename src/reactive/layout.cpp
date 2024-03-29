#include "layout.h"

#include "widget/providebuildparams.h"
#include "widget/setsizehint.h"
#include "widget/transform.h"
#include "widget/addwidgets.h"
#include "widget/instancemodifier.h"

#include "avg/rendertree.h"
#include "widget/widget.h"

namespace reactive
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

    auto hintsSignal = share(
            signal::combine(std::move(hints))
            );

    auto widget = widget::makeWidgetWithSize(
            [](auto size, auto obbMap, auto hintsSignal, auto builders)
            {
                auto obbs = share(signal::map(obbMap, std::move(size), hintsSignal));

                size_t index = 0;
                auto widgets = btl::fmap(builders, [&index, &obbs](auto&& f)
                        -> AnySignal<widget::Instance>
                    {
                        auto t = signal::map([index](
                                    std::vector<avg::Obb> const& obbs)
                                {
                                    return obbs.at(index).getTransform();
                                }, obbs);

                        auto size = signal::map([index](
                                    std::vector<avg::Obb> const& obbs)
                                {
                                    return obbs.at(index).getSize();
                                }, obbs);

                        auto builder = f.clone()
                            | widget::transformBuilder(std::move(t));

                        ++index;

                        return std::move(builder)(std::move(size)).getInstance();
                    });

                return widget::makeWidget()
                    | widget::addWidgets(std::move(widgets))
                    ;
            }
            ,
            std::move(obbMap),
            hintsSignal,
            std::move(builders)
            );

    return std::move(widget)
        | widget::setSizeHint(signal::map(std::move(sizeHintMap), hintsSignal));
}

} // anonymous namespace

widget::AnyWidget layout(SizeHintMap sizeHintMap,
        ObbMap obbMap, std::vector<widget::AnyWidget> widgets)
{
    return makeWidget([](widget::BuildParams const& params,
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
    widget::provideBuildParams(),
    std::move(sizeHintMap),
    std::move(obbMap),
    std::move(widgets)
    );
}
} // namespace reactive

