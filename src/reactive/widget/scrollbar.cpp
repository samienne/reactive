#include "widget/scrollbar.h"

#include "widget/margin.h"
#include "widget/onpointerdown.h"
#include "widget/onpointerup.h"
#include "widget/onpointermove.h"
#include "widget/ondraw.h"
#include "widget/onhover.h"
#include "widget/settheme.h"
#include "widget/instancemodifier.h"
#include "widget/setsizehint.h"
#include "widget/providetheme.h"

#include "reactive/shapes.h"
#include "reactive/simplesizehint.h"

#include "send.h"

#include <avg/pathbuilder.h>

namespace reactive::widget
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
            widget::Theme const& theme, float amount, float handleSize,
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
            widget::Theme const& theme, float amount, float handleSize,
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
            signal::InputHandle<std::optional<avg::Vector2f>> downHandle,
            signal::Signal<T, avg::Vector2f> sizeSignal,
            signal::Signal<U, float> amountSignal,
            signal::Signal<V, float> handleSizeSignal)
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
            return signal::constant(simpleSizeHint(main, aux));
        else
            return signal::constant(simpleSizeHint(aux, main));
    }

    template <bool IsHorizontal>
    auto makeScrollBar(
        signal::InputHandle<float> scrollHandle,
        signal::AnySignal<float> amount,
        signal::AnySignal<float> handleSize)
    {
        return makeWidgetWithSize([](auto size, auto theme, auto scrollHandle,
                    auto amount, auto handleSize)
        {
            auto downOffset = signal::makeInput<std::optional<avg::Vector2f>>(std::nullopt);
            auto isDown = downOffset.signal.map([](auto v) { return v.has_value(); });
            auto hover = signal::makeInput(false);

            auto sliderObb = merge(size.clone(), amount, handleSize).map([]
                    (avg::Vector2f size, float amount, float handleSize)
                    {
                        return avg::Obb(getSliderRect<IsHorizontal>(
                                    size,
                                    amount,
                                    handleSize));
                    });

            return makeWidget()
                | onHover(std::move(sliderObb), hover.handle)
                | onPointerDown(scrollPointerDown<IsHorizontal>(
                            downOffset.handle, size.clone(), amount, handleSize)
                        )
                | onPointerUp([handle=downOffset.handle]() mutable
                    {
                        handle.set(std::nullopt);
                        return EventResult::accept;
                    })
                | onPointerMove(merge(downOffset.signal, size.clone(), handleSize)
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
                | onDraw(
                        drawScrollBar<IsHorizontal>,
                        std::move(theme),
                        amount,
                        handleSize,
                        std::move(hover.signal),
                        std::move(isDown)
                        )
                ;
        },
        provideTheme(),
        scrollHandle,
        std::move(amount),
        std::move(handleSize)
        );
    }
} // anonymous namespace

template <bool IsHorizontal>
AnyWidget scrollBar(
        signal::InputHandle<float> scrollHandle,
        signal::AnySignal<float> amount,
        signal::AnySignal<float> handleSize)
{
    return makeScrollBar<IsHorizontal>(scrollHandle, amount, handleSize)
        | widget::margin(signal::constant(5.0f))
        | setSizeHint(getScrollBarSizeHint<IsHorizontal>())
        ;
}

AnyWidget hScrollBar(
        signal::InputHandle<float> handle,
        signal::AnySignal<float> amount,
        signal::AnySignal<float> handleSize)
{
    return scrollBar<true>(std::move(handle), std::move(amount).share(),
            std::move(handleSize).map([](float v)
                    {
                        return std::min(1.0f, v);
                    }).share());
}

AnyWidget vScrollBar(
        signal::InputHandle<float> handle,
        signal::AnySignal<float> amount,
        signal::AnySignal<float> handleSize)
{
    return scrollBar<false>(std::move(handle), std::move(amount).share(),
            std::move(handleSize).map([](float v)
                    {
                        return std::min(1.0f, v);
                    }).share());
}

} // namespace reactive::widget

