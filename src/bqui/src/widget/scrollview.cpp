#include "bqui/widget/scrollview.h"

#include "bqui/widget/scrollbar.h"
#include "bqui/widget/widget.h"
#include "bqui/widget/bin.h"

#include "bqui/modifier/frame.h"
#include "bqui/modifier/tracksize.h"
#include "bqui/modifier/onpointerdown.h"
#include "bqui/modifier/onpointerup.h"
#include "bqui/modifier/onpointermove.h"
#include "bqui/modifier/setsizehint.h"
#include "bqui/modifier/transform.h"

#include "bqui/simplesizehint.h"
#include "bqui/widget/hbox.h"
#include "bqui/widget/vbox.h"
#include "bqui/widget/filler.h"

#include "bqui/sendvalue.h"

#include "bqui/provider/providebuildparams.h"

namespace bqui::widget
{

AnyWidget scrollView(AnyWidget widget)
{
    return makeWidget([](BuildParams params, auto widget)
    {
        auto builder = std::move(widget)(std::move(params));

        auto viewSize = bq::signal::makeInput(avg::Vector2f(10.0f, 200.0f));
        auto x = bq::signal::makeInput(0.5f);
        auto y = bq::signal::makeInput(0.5f);

        auto contentSize = builder.getSizeHint().map([](auto hint)
                {
                    float w = hint.getWidth()[1];
                    float h = hint.getHeightForWidth(w)[1];

                    return avg::Vector2f(w, h);
                }).share();

        auto hHandleSize = merge(contentSize, viewSize.signal)
            .map([](avg::Vector2f contentSize, avg::Vector2f viewSize)
                {
                    if (contentSize[0] < 0.0001f)
                        return 1.0f;

                    return viewSize[0] / contentSize[0];
                });

        auto vHandleSize = merge(contentSize, viewSize.signal)
            .map([](avg::Vector2f contentSize, avg::Vector2f viewSize)
                {
                    if (contentSize[1] < 0.0001f)
                        return 1.0f;

                    return viewSize[1] / contentSize[1];
                });

        auto dragOffset = bq::signal::makeInput(avg::Vector2f());
        auto scrollPos = bq::signal::makeInput<std::optional<avg::Vector2f>>(std::nullopt);

        auto t = merge(x.signal, y.signal, contentSize, viewSize.signal)
            .map([](float x, float y, avg::Vector2f contentSize, avg::Vector2f viewSize)
                {
                    return avg::translate(
                            x * -(contentSize[0] - viewSize[0]),
                            (1.0f - y) * (contentSize[1] - viewSize[1]));
                });

        auto contentWidget = makeWidgetFromBuilder(std::move(builder))
            | modifier::transform(std::move(t))
            ;

        auto view = bin(std::move(contentWidget), contentSize)
            | modifier::setSizeHint(bq::signal::constant(simpleSizeHint(
                {{100, 400, 10000}},
                {{100, 800, 10000}}
                )))
            | modifier::trackSize(viewSize.handle)
            | modifier::onPointerDown(merge(x.signal, y.signal).bindToFunction(
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
                    })
                    )
            | modifier::onPointerMove(merge(dragOffset.signal, viewSize.signal,
                        contentSize, scrollPos.signal).bindToFunction(
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
                    }))
            | modifier::onPointerUp([scrollPosHandle=scrollPos.handle]
                    (PointerButtonEvent const&) mutable
                    {
                        scrollPosHandle.set(std::nullopt);
                        return EventResult::reject;
                    })
            | modifier::frame()
            ;

        auto makeBox = []()
        {
            return bq::signal::constant(simpleSizeHint(25.0f, 25.0f));
        };

        return vbox({
                hbox({ std::move(view), widget::vScrollBar(
                            y.handle, y.signal, std::move(vHandleSize))}),
                hbox({ hScrollBar(x.handle, x.signal, std::move(hHandleSize)),
                        hfiller() | modifier::setSizeHint(makeBox())
                        })
                });
    },
    provider::provideBuildParams(),
    std::move(widget)
    );
}

}

