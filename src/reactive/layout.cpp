#include "layout.h"

#include "widget/setsizehint.h"
#include "widget/transform.h"
#include "widget/addwidgets.h"
#include "widget/instancemodifier.h"

#include "avg/rendertree.h"

namespace reactive
{

widget::AnyBuilder layout(SizeHintMap sizeHintMap, ObbMap obbMap,
        std::vector<widget::AnyBuilder> builders)
{
    auto hints = btl::fmap(builders, [](auto const& builder)
            {
                return builder.getSizeHint();
            });

    auto hintsSignal = share(
            signal::combine(std::move(hints))
            );

    auto transformer = widget::makeSharedInstanceSignalModifier([]
            (auto widget, auto obbMap, auto hintsSignal, auto builders)
            {
                auto size = signal::map(&widget::Instance::getSize, widget);

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
                            | widget::transform(std::move(t));

                        ++index;

                        return std::move(builder)(std::move(size));
                    });

                return std::move(widget)
                    | widget::addWidgets(std::move(widgets))
                    ;
            }
            ,
            std::move(obbMap),
            hintsSignal,
            std::move(builders)
            );

    return widget::makeBuilder()
        | std::move(transformer)
        | widget::setSizeHint(signal::map(std::move(sizeHintMap), hintsSignal));
}

} // namespace reactive

