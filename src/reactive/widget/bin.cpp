#include "widget/bin.h"

#include "widget/clip.h"

namespace reactive::widget
{

WidgetFactory bin(WidgetFactory f, BinSizeHintMap sizeHintMap,
        BinObbMap obbMap)
{
    auto sizeHint = signal::share(f.getSizeHint());

    auto obb = obbMap(sizeHint);
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
        | widget::clip()
        | setSizeHint(signal::map(sizeHintMap, std::move(sizeHint)))
        ;
}
} // namespace reactive::widget
