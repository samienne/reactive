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
#include "modifier/setsizehint.h"
#include "bqui/modifier/transform.h"

#include "bqui/simplesizehint.h"
#include "bqui/widget/hbox.h"
#include "bqui/widget/vbox.h"

#include "bqui/sendvalue.h"

#include "bqui/provider/providebuildparams.h"

#include <bq/signal/constant.h>

#include <algorithm>
#include <optional>

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

        // The content is clipped to the viewport, so its size is intrinsic to the
        // content: its extent is the natural of its own pure band (the grid
        // solved at its own size). An axis whose band states no natural takes
        // the viewport's extent, held within the band's min and max, so content
        // that only fills scrolls no further than its bounds demand. The height
        // reads the band at an empty width solution, so this is exact only where
        // the height is width-independent; a genuinely reflowing content would
        // measure at width zero -- a known limitation, not yet handled.
        PureLayout const layout = builder.getPureLayout();

        auto extentOf = [](Constraints const& c, float viewport)
        {
            if (c.natural)
                return c.natural->value;

            float extent = viewport;
            if (c.max)
                extent = std::min(extent, *c.max);
            if (c.min)
                extent = std::max(extent, *c.min);

            return extent;
        };

        auto width = layout.getWidth();
        auto height = layout.getHeightForWidth(
                bq::signal::constant(LayoutSolution()));

        bq::signal::AnySignal<avg::Vector2f> contentSizeSignal = merge(
                std::move(width), std::move(height), viewSize.signal).map(
                [extentOf](Constraints const& w, Constraints const& h,
                    avg::Vector2f viewport)
                {
                    return avg::Vector2f(extentOf(w, viewport[0]),
                            extentOf(h, viewport[1]));
                });

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
            // publishes its own pure band -- the viewport's preferred size,
            // flexing on both axes -- and a pure-region parent sizes the scroll
            // view to its slot rather than the content.
            | modifier::defaultSize(avg::Vector2f(400.0f, 800.0f))
            | modifier::minSize(avg::Vector2f(100.0f, 100.0f))
            | modifier::growWidth()
            | modifier::growHeight()
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

        // A box carries a child's flex outward only along its own axis, so the
        // view's fill is restated on the rows and the column across them.
        return vbox({
                hbox({ std::move(view), widget::vScrollBar(
                            y.handle, y.signal, std::move(vHandleSize))})
                    | modifier::fill(),
                hbox({ hScrollBar(x.handle, x.signal, std::move(hHandleSize)),
                        std::move(corner)
                        })
                })
            | modifier::fill();
    },
    provider::provideBuildParams(),
    std::move(widget)
    );
}

}

