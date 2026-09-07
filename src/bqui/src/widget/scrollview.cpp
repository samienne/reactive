#include "bqui/widget/scrollview.h"

#include "constraintbox.h"

#include "bqui/widget/scrollbar.h"
#include "bqui/widget/widget.h"
#include "bqui/widget/bin.h"

#include "bqui/modifier/constraintsize.h"
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

#include "bqui/sendvalue.h"

#include "bqui/provider/providebuildparams.h"

#include <bq/signal/constant.h>

#include <optional>

namespace bqui::widget
{

AnyWidget scrollView(AnyWidget widget)
{
    return makeWidget([](BuildParams params, auto widget)
    {
        bool const inPureRegion = pureSolver(params);

        auto builder = std::move(widget)(std::move(params));

        auto viewSize = bq::signal::makeInput(avg::Vector2f(10.0f, 200.0f));
        auto x = bq::signal::makeInput(0.5f);
        auto y = bq::signal::makeInput(0.5f);

        auto hintSize = builder.getSizeHint().map([](auto hint)
                {
                    float w = hint.getWidth().extent.natural;
                    float h = hint.getHeightForWidth(w).extent.natural;

                    return avg::Vector2f(w, h);
                }).share();

        // The content is clipped to the viewport, so its size is intrinsic to the
        // content. In a pure-solver region a pure content no longer fills in a
        // SizeHint, so its extent is the natural of its own pure band (the grid
        // solved at its own size); the height reads its band at an empty width
        // solution, so a genuinely width-dependent content falls back to the
        // bridged SizeHint on the axis whose band carries no natural.
        bq::signal::AnySignal<avg::Vector2f> contentSizeSignal = hintSize.clone();
        if (inPureRegion && builder.getPureLayout())
        {
            PureLayout const layout = *builder.getPureLayout();

            auto naturalOf = [](Constraints const& c) -> std::optional<float>
            {
                if (c.natural)
                    return c.natural->value;

                return std::nullopt;
            };

            auto width = layout.getWidth().map(naturalOf);
            auto height = layout.getHeightForWidth(
                    bq::signal::constant(LayoutSolution())).map(naturalOf);

            contentSizeSignal = merge(std::move(width), std::move(height),
                    hintSize.clone()).map(
                    [](std::optional<float> w, std::optional<float> h,
                        avg::Vector2f hint)
                    {
                        return avg::Vector2f(w.value_or(hint[0]),
                                h.value_or(hint[1]));
                    });
        }

        auto contentSize = std::move(contentSizeSignal).share();

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
                Band{100, 400, 10000, 1},
                Band{100, 800, 10000, 1}
                )))
            // The firewall stops the content's size propagating up, so the view
            // publishes its own pure band -- the viewport's preferred size, from
            // the SizeHint above -- and a pure-region parent sizes the scroll view
            // to the viewport rather than the content.
            | modifier::defaultSize()
            | modifier::trackSize(viewSize.handle)
            | modifier::onPointerDown(merge(x.signal, y.signal).bindFirst(
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
                        contentSize, scrollPos.signal).bindFirst(
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

        // The corner where the bars meet is a fixed vScrollBar-thickness square,
        // not a filler: a pure hScrollBar flexes along the row, so a competing
        // filler corner would split the row's slack with it. A fixed corner lets
        // the bar take the whole width but the thickness. Banded, the same fixed
        // 25x25 SizeHint holds, so the banded row is unchanged.
        auto corner = makeWidget()
            | modifier::setSizeHint(makeBox())
            | modifier::fixedSize(bq::signal::constant(avg::Vector2f(25.0f, 25.0f)))
            ;

        return vbox({
                hbox({ std::move(view), widget::vScrollBar(
                            y.handle, y.signal, std::move(vHandleSize))}),
                hbox({ hScrollBar(x.handle, x.signal, std::move(hHandleSize)),
                        std::move(corner)
                        })
                });
    },
    provider::provideBuildParams(),
    std::move(widget)
    );
}

}

