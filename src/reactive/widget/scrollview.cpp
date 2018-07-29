#include "widget/scrollview.h"

#include "widget/frame.h"
#include "widget/scrollbar.h"
#include "widget/bin.h"
#include "reactive/hbox.h"
#include "reactive/vbox.h"
#include "reactive/filler.h"

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
    auto x = signal::input(0.5f);
    auto y = signal::input(0.5f);

    auto t = signal::map([](float x, float y,
                avg::Vector2f contentSize, avg::Vector2f viewSize)
            {
                return avg::translate(
                        x * -(contentSize[0] - viewSize[0]),
                        (1.0f - y) * (contentSize[1] - viewSize[1]));
            }, x.signal, y.signal, contentSize.signal, viewSize.signal);

    auto hHandleSize = signal::map([](avg::Vector2f contentSize,
                avg::Vector2f viewSize)
            {
                if (contentSize[0] < 0.0001f)
                    return 1.0f;

                return viewSize[0] / contentSize[0];
            }, contentSize.signal, viewSize.signal);

    auto vHandleSize = signal::map([](avg::Vector2f contentSize,
                avg::Vector2f viewSize)
            {
                if (contentSize[1] < 0.0001f)
                    return 1.0f;

                return viewSize[1] / contentSize[1];
            }, contentSize.signal, viewSize.signal);

    auto f2 = std::move(f)
        | trackSize(contentSize.handle)
        | transform(t.clone())
        ;

    auto view = bin(std::move(f2), hintMap,
            [contentSize=std::move(contentSize.signal),
            viewSize=std::move(viewSize.signal)]
            (Signal<SizeHint> sizeHint)
            {
                return obbMap(std::move(sizeHint), btl::clone(contentSize),
                        btl::clone(viewSize));
            })
            | trackSize(viewSize.handle)
            | widget::frame()
        ;

    auto makeBox = []()
    {
        return signal::constant(simpleSizeHint(25.0f, 25.0f));
    };

    return vbox({
            hbox({ std::move(view), widget::vScrollBar(
                        y.handle, y.signal, std::move(vHandleSize))}),
            hbox({ hScrollBar(x.handle, x.signal, std::move(hHandleSize)),
                    hfiller() | setSizeHint(makeBox())
                    })
            });
}

} // namespace reactive::widget

