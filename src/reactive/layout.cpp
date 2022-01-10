#include "layout.h"

#include "widget/transform.h"
#include "widget/addwidgets.h"
#include "widget/instancemodifier.h"

#include "avg/rendertree.h"

namespace reactive
{

WidgetFactory layout(SizeHintMap sizeHintMap, ObbMap obbMap,
        std::vector<WidgetFactory> factories)
{
    auto hints = btl::fmap(factories, [](auto const& factory)
            {
                return factory.getSizeHint();
            });

    auto hintsSignal = share(
            signal::combine(std::move(hints))
            );

    auto transformer = widget::makeSharedInstanceSignalModifier([]
            (auto widget, auto obbMap, auto hintsSignal, auto factories)
            {
                auto size = signal::map(&widget::Instance::getSize, widget);

                auto obbs = share(signal::map(obbMap, std::move(size), hintsSignal));

                size_t index = 0;
                auto widgets = btl::fmap(factories, [&index, &obbs](auto&& f)
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

                        auto factory = f.clone()
                            | widget::transform(std::move(t));

                        ++index;

                        return std::move(factory)(std::move(size));
                    });

                return std::move(widget)
                    | widget::addWidgets(std::move(widgets))
                    ;
            }
            ,
            std::move(obbMap),
            hintsSignal,
            std::move(factories)
            );

    return makeWidgetFactory()
        | std::move(transformer)
        | setSizeHint(signal::map(std::move(sizeHintMap), hintsSignal));
}

} // namespace reactive

