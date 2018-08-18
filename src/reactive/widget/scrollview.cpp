#include "widget/scrollview.h"

#include "widget/frame.h"
#include "widget/scrollbar.h"
#include "widget/bin.h"
#include "reactive/sendvalue.h"
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

    auto dragOffset = signal::input(avg::Vector2f());
    auto scrollPos = signal::input<btl::option<avg::Vector2f>>(btl::none);

    auto f2 = std::move(f)
        | trackSize(contentSize.handle)
        | transform(t.clone())
        | onPointerDown(signal::mapFunction(
                [dragOffsetHandle=dragOffset.handle,
                scrollPosHandle=scrollPos.handle
                ]
                (avg::Vector2f contentSize, float x, float y,
                 PointerButtonEvent const& e) mutable
                {
                    avg::Vector2f center(contentSize[0] / 2.0f,
                            contentSize[1] / 2.0f);

                    if (e.button == 1)
                    {
                        dragOffsetHandle.set(e.pos );//- center);
                        scrollPosHandle.set(btl::just(avg::Vector2f(x, y)));
                    }

                    return EventResult::possible;
                }, contentSize.signal, x.signal, y.signal))
        | onPointerMove(signal::mapFunction(
                [xHandle=x.handle, yHandle=y.handle]
                (avg::Vector2f dragOffset, avg::Vector2f viewSize,
                    avg::Vector2f contentSize,
                    btl::option<avg::Vector2f> scrollPos,
                    PointerMoveEvent const& e) mutable
                {
                    if (!scrollPos.valid())
                        return EventResult::possible;

                    float hLen = contentSize[0] - viewSize[0];
                    float vLen = contentSize[1] - viewSize[1];

                    float x = -(e.pos[0] - dragOffset[0]) / hLen + (*scrollPos)[0];
                    float y = -(e.pos[1] - dragOffset[1]) / vLen + (*scrollPos)[1];

                    xHandle.set(std::max(0.0f, std::min(x, 1.0f)));
                    yHandle.set(std::max(0.0f, std::min(y, 1.0f)));

                    return EventResult::exclusive;
                }, dragOffset.signal, viewSize.signal, contentSize.signal,
                scrollPos.signal))
        | onPointerUp([scrollPosHandle=scrollPos.handle]
                (PointerButtonEvent const&) mutable
                {
                    scrollPosHandle.set(btl::none);
                    return EventResult::reject;
                });
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

