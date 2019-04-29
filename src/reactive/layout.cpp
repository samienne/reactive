#include "layout.h"

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

    auto map = [obbMap=std::move(obbMap),
            hintsSignal, factories=btl::cloneOnCopy(std::move(factories))]
                (auto w)
        // -> Widget
    {
        auto obbs = share(signal::map(obbMap, w.getSize(), hintsSignal));

        size_t index = 0;
        auto widgets = btl::fmap(*factories, [&index, &obbs](auto&& f)
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
                    | transform(std::move(t));

                ++index;

                return std::move(factory)(std::move(size));
            });

        return std::move(w)
            | addWidgets(std::move(widgets));
    };

    return makeWidgetFactory()
        | widgetMap(std::move(map))
        | setSizeHint(signal::map(std::move(sizeHintMap), hintsSignal));
}

} // namespace layout

