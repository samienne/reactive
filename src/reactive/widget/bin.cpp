#include "widget/bin.h"

namespace reactive::widget
{

namespace
{
} // anonymous namespace

WidgetFactory bin(WidgetFactory f, BinSizeHintMap sizeHintMap,
        BinObbMap obbMap)
{
    auto sizeHint = signal::share(f.getSizeHint());

    auto obb = signal::map(obbMap, sizeHint);
    auto splitted = signal::split(signal::map([](avg::Obb const& obb)
                {
                    return std::make_tuple(obb.getSize(), obb.getTransform());
                },
                std::move(obb)));

    auto size = std::move(std::get<0>(splitted));
    auto t = std::move(std::get<1>(splitted));

    auto newF = std::move(f)|transform(std::move(t));

    return makeWidgetFactory()
        | addWidget(Widget(std::move(newF)(std::move(size))))
        | setSizeHint(signal::map(sizeHintMap, std::move(sizeHint)))
        ;
}
} // namespace reactive::widget
