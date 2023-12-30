#include "widget/scrollview.h"

#include "widget/providebuildparams.h"
#include "widget/frame.h"
#include "widget/scrollbar.h"
#include "widget/bin.h"
#include "widget/tracksize.h"
#include "widget/onpointerdown.h"
#include "widget/onpointerup.h"
#include "widget/onpointermove.h"
#include "widget/instancemodifier.h"
#include "widget/setsizehint.h"

#include "reactive/simplesizehint.h"
#include "reactive/sendvalue.h"
#include "reactive/hbox.h"
#include "reactive/vbox.h"
#include "reactive/filler.h"

namespace reactive::widget
{

AnyWidget scrollView(AnyWidget widget)
{
    return makeWidget([](BuildParams params, auto widget)
    {
        auto builder = std::move(widget)(std::move(params));

        auto viewSize = signal::input(avg::Vector2f(10.0f, 200.0f));
        auto x = signal::input(0.5f);
        auto y = signal::input(0.5f);

        auto contentSize = signal::share(signal::map([](auto hint)
                {
                    float w = hint.getWidth()[1];
                    float h = hint.getHeightForWidth(w)[1];

                    return avg::Vector2f(w, h);
                },
                builder.getSizeHint()
                ));

        auto hHandleSize = signal::map([](avg::Vector2f contentSize,
                    avg::Vector2f viewSize)
                {
                    if (contentSize[0] < 0.0001f)
                        return 1.0f;

                    return viewSize[0] / contentSize[0];
                }, contentSize, viewSize.signal);

        auto vHandleSize = signal::map([](avg::Vector2f contentSize,
                    avg::Vector2f viewSize)
                {
                    if (contentSize[1] < 0.0001f)
                        return 1.0f;

                    return viewSize[1] / contentSize[1];
                }, contentSize, viewSize.signal);

        auto dragOffset = signal::input(avg::Vector2f());
        auto scrollPos = signal::input<std::optional<avg::Vector2f>>(std::nullopt);

        auto t = signal::map([](float x, float y,
                    avg::Vector2f contentSize, avg::Vector2f viewSize)
                {
                    return avg::translate(
                            x * -(contentSize[0] - viewSize[0]),
                            (1.0f - y) * (contentSize[1] - viewSize[1]));
                }, x.signal, y.signal, contentSize, viewSize.signal);

        auto builder2 = std::move(builder)
            | transform(std::move(t))
            ;

        auto view = makeWidget()
            | bin(std::move(builder2), contentSize)
            | setSizeHint(signal::constant(simpleSizeHint(
                {{100, 400, 10000}},
                {{100, 800, 10000}}
                )))
            | trackSize(viewSize.handle)
            | onPointerDown(signal::mapFunction(
                    [dragOffsetHandle=dragOffset.handle,
                    scrollPosHandle=scrollPos.handle
                    ]
                    (float x, float y,
                    PointerButtonEvent const& e) mutable
                    {
                        if (e.button == 1)
                        {
                            dragOffsetHandle.set(e.pos );
                            scrollPosHandle.set(std::make_optional(avg::Vector2f(x, y)));
                        }

                        return EventResult::possible;
                    }, x.signal, y.signal))
            | onPointerMove(signal::mapFunction(
                    [xHandle=x.handle, yHandle=y.handle]
                    (avg::Vector2f dragOffset, avg::Vector2f viewSize,
                        avg::Vector2f contentSize,
                        std::optional<avg::Vector2f> scrollPos,
                        PointerMoveEvent const& e) mutable
                    {
                        if (!scrollPos.has_value())
                            return EventResult::possible;

                        float hLen = contentSize[0] - viewSize[0];
                        float vLen = contentSize[1] - viewSize[1];

                        float x = -(e.pos[0] - dragOffset[0]) / hLen + (*scrollPos)[0];
                        float y = -(e.pos[1] - dragOffset[1]) / vLen + (*scrollPos)[1];

                        xHandle.set(std::max(0.0f, std::min(x, 1.0f)));
                        yHandle.set(std::max(0.0f, std::min(y, 1.0f)));

                        return EventResult::accept;
                    }, dragOffset.signal, viewSize.signal, contentSize,
                    scrollPos.signal))
            | onPointerUp([scrollPosHandle=scrollPos.handle]
                    (PointerButtonEvent const&) mutable
                    {
                        scrollPosHandle.set(std::nullopt);
                        return EventResult::reject;
                    })
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
    },
    widget::provideBuildParams(),
    std::move(widget)
    );
}

} // namespace reactive::widget

