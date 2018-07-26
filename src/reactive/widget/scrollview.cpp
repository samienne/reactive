#include "widget/scrollview.h"

#include "widget/bin.h"

namespace reactive::widget
{

namespace
{
    SizeHint hintMap(SizeHint /*hint*/)
    {
        return simpleSizeHint(
                {{100, 200, 10000}},
                {{100, 400, 10000}}
                );
    }

    Signal<avg::Obb> obbMap(Signal<SizeHint> hint,
            Signal<avg::Vector2f> contentSize,
            Signal<avg::Vector2f> viewSize)
    {
        return signal::map(
                [](SizeHint hint, avg::Vector2f contentSize,
                    avg::Vector2f viewSize)
                {
                    float w = hint()[1];
                    float h = hint(w)[1];

                    float offY = contentSize[1] - viewSize[1];

                    return avg::translate(0.0f, -offY)
                        * avg::Obb(avg::Vector2f(w, h));
                },
                std::move(hint),
                std::move(contentSize),
                std::move(viewSize));
    }
} // anonymous namespace

WidgetFactory scrollView(WidgetFactory f)
{
    auto contentSize = signal::input(avg::Vector2f());
    auto viewSize = signal::input(avg::Vector2f(10.0f, 200.0f));

    auto f2 = std::move(f)
        | trackSize(contentSize.handle)
        ;

    return bin(std::move(f2), hintMap,
            [contentSize=std::move(contentSize.signal),
            viewSize=std::move(viewSize.signal)]
            (Signal<SizeHint> sizeHint)
            {
                return obbMap(std::move(sizeHint), btl::clone(contentSize),
                        btl::clone(viewSize));
            })
        | trackSize(viewSize.handle)
        ;
}

} // namespace reactive::widget

