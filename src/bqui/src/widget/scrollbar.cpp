#include "bqui/widget/scrollbar.h"

#include "bqui/modifier/margin.h"
#include "bqui/modifier/onpointerdown.h"
#include "bqui/modifier/onpointerup.h"
#include "bqui/modifier/onpointermove.h"
#include "bqui/modifier/ondraw.h"
#include "bqui/modifier/onhover.h"
#include "bqui/modifier/settheme.h"
#include "bqui/modifier/instancemodifier.h"
#include "bqui/modifier/setsizehint.h"

#include "bqui/provider/providetheme.h"

#include "bqui/shapes.h"
#include "bqui/simplesizehint.h"
#include "bqui/send.h"

#include <avg/pathbuilder.h>

namespace bqui::widget
{

namespace
{
    template <bool IsHorizontal>
    avg::Rect getSliderRect(avg::Vector2f size, float amount,
            float handleSize)
    {
        int x = IsHorizontal ? 0 : 1;
        int y = IsHorizontal ? 1 : 0;

        float handleLen = std::max(10.0f, size[x] * handleSize);
        float coords[2] = {
            (size[x] - handleLen) * amount,
            size[y] / 2.0f - 10.0f
        };

        float sizes[2] = { handleLen, 20.0f };

        return avg::Rect(
                avg::Vector2f(coords[x], coords[y]),
                avg::Vector2f(sizes[x], sizes[y])
                );
    }

    avg::Path rectToPath(pmr::memory_resource* memory, avg::Rect const& r)
    {
        return makePathFromRect(memory, r, 10.0f);
    }

    template <bool IsHorizontal>
    avg::Drawing drawSlider(
            avg::DrawContext const& drawContext, avg::Vector2f size,
            Theme const& theme, float amount, float handleSize,
            bool hover, bool isDown)
    {
        avg::Color bgColor = theme.getBackground();
        avg::Color fgColor = theme.getSecondary();

        if (isDown)
        {
            fgColor = theme.getEmphasized();
            bgColor = theme.getSecondary();
        }
        else if (hover)
        {
            bgColor = theme.getBackgroundHighlight();
        }

        avg::Pen pen(fgColor, 1.0f);
        avg::Brush brush(bgColor);

        avg::Path slider(rectToPath(
                    drawContext.getResource(),
                    getSliderRect<IsHorizontal>(size, amount, handleSize)
                    ));

        return avg::Shape(std::move(slider))
            .fillAndStroke(brush, pen);
    }

    template <bool IsHorizontal>
    avg::Path getScrollLinePath(avg::DrawContext const& drawContext, avg::Vector2f size)
    {

        if (IsHorizontal)
        {
            return drawContext.pathBuilder()
                .start(0.0f, size[1] / 2.0f)
                .lineTo(size[0], size[1] / 2.0f)
                .build();
        }
        else
        {
            return drawContext.pathBuilder()
                .start(size[0] / 2.0f, 0.0f)
                .lineTo(size[0] / 2.0f, size[1])
                .build();
        }
    }

    template <bool IsHorizontal>
    avg::Drawing drawScrollBar(avg::DrawContext const& drawContext, avg::Vector2f size,
            Theme const& theme, float amount, float handleSize,
            bool hover, bool isDown)
    {
        avg::Path line = getScrollLinePath<IsHorizontal>(drawContext, size);

        avg::Pen pen(theme.getBackgroundHighlight(), 1.0f);

        return avg::Shape(std::move(line)).stroke(pen)
            + drawSlider<IsHorizontal>(drawContext, size, theme, amount,
                    handleSize, hover, isDown)
            ;
    }

    template <bool IsHorizontal, typename T, typename U, typename V>
    auto scrollPointerDown(
            bq::signal::InputHandle<std::optional<avg::Vector2f>> downHandle,
            bq::signal::Signal<T, avg::Vector2f> sizeSignal,
            bq::signal::Signal<U, float> amountSignal,
            bq::signal::Signal<V, float> handleSizeSignal)
    {
        return merge(
                std::move(sizeSignal),
                amountSignal,
                std::move(handleSizeSignal)
            ).bindToFunction([downHandle=std::move(downHandle)]
            (avg::Vector2f size, float amount, float handleSize,
             PointerButtonEvent e) mutable
            {
                auto r = getSliderRect<IsHorizontal>(size, amount,
                        handleSize);

                if (e.button == 1 && r.contains(e.pos))
                    downHandle.set(e.pos-r.getCenter());

                return EventResult::possible;
            });
    }

    template <bool IsHorizontal>
    auto getScrollBarSizeHint()
    {
        std::array<float, 3> main{{50, 100, 10000}};
        std::array<float, 3> aux{{25, 25, 25}};

        if (IsHorizontal)
            return bq::signal::constant(simpleSizeHint(main, aux));
        else
            return bq::signal::constant(simpleSizeHint(aux, main));
    }

    template <bool IsHorizontal>
    auto makeScrollBar(
        bq::signal::InputHandle<float> scrollHandle,
        bq::signal::AnySignal<float> amount,
        bq::signal::AnySignal<float> handleSize)
    {
        return makeWidgetWithSize([](auto size, auto theme, auto scrollHandle,
                    auto amount, auto handleSize)
        {
            auto downOffset = bq::signal::makeInput<std::optional<avg::Vector2f>>(std::nullopt);
            auto isDown = downOffset.signal.map([](auto v) { return v.has_value(); });
            auto hover = bq::signal::makeInput(false);

            auto sliderObb = merge(size.clone(), amount, handleSize).map([]
                    (avg::Vector2f size, float amount, float handleSize)
                    {
                        return avg::Obb(getSliderRect<IsHorizontal>(
                                    size,
                                    amount,
                                    handleSize));
                    });

            return makeWidget()
                | modifier::onHover(std::move(sliderObb), hover.handle)
                | modifier::onPointerDown(scrollPointerDown<IsHorizontal>(
                            downOffset.handle, size.clone(), amount, handleSize)
                        )
                | modifier::onPointerUp([handle=downOffset.handle]() mutable
                    {
                        handle.set(std::nullopt);
                        return EventResult::accept;
                    })
                | modifier::onPointerMove(merge(downOffset.signal, size.clone(),
                            handleSize)
                    .bindToFunction([scrollHandle]
                    (std::optional<avg::Vector2f> downOffset,
                        avg::Vector2f size, float handleSize,
                        PointerMoveEvent const& e) mutable -> EventResult
                    {
                        if (!downOffset.has_value())
                            return EventResult::possible;

                        if (handleSize == 1.0f)
                            return EventResult::possible;

                        int const axis = IsHorizontal ? 0 : 1;
                        float offset = (*downOffset)[axis];
                        float handleLen  = std::max(10.0f, handleSize * size[axis]);
                        float lineLen = size[axis] - handleLen;

                        float pos = e.pos[axis] - offset - handleLen / 2.0f;

                        float len = pos / lineLen;

                        scrollHandle.set(std::max(0.0f, std::min(len, 1.0f)));
                        return EventResult::accept;
                    }))
                | modifier::onDraw(
                        drawScrollBar<IsHorizontal>,
                        std::move(theme),
                        amount,
                        handleSize,
                        std::move(hover.signal),
                        std::move(isDown)
                        )
                ;
        },
        provider::provideTheme(),
        scrollHandle,
        std::move(amount),
        std::move(handleSize)
        );
    }
} // anonymous namespace

template <bool IsHorizontal>
AnyWidget scrollBar(
        bq::signal::InputHandle<float> scrollHandle,
        bq::signal::AnySignal<float> amount,
        bq::signal::AnySignal<float> handleSize)
{
    return makeScrollBar<IsHorizontal>(scrollHandle, amount, handleSize)
        | modifier::margin(bq::signal::constant(5.0f))
        | modifier::setSizeHint(getScrollBarSizeHint<IsHorizontal>())
        ;
}

AnyWidget hScrollBar(
        bq::signal::InputHandle<float> handle,
        bq::signal::AnySignal<float> amount,
        bq::signal::AnySignal<float> handleSize)
{
    return scrollBar<true>(std::move(handle), std::move(amount).share(),
            std::move(handleSize).map([](float v)
                    {
                        return std::min(1.0f, v);
                    }).share());
}

AnyWidget vScrollBar(
        bq::signal::InputHandle<float> handle,
        bq::signal::AnySignal<float> amount,
        bq::signal::AnySignal<float> handleSize)
{
    return scrollBar<false>(std::move(handle), std::move(amount).share(),
            std::move(handleSize).map([](float v)
                    {
                        return std::min(1.0f, v);
                    }).share());
}

}

