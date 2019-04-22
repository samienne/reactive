#include "widget/scrollbar.h"

#include "widget/margin.h"

#include "reactive/bindwidgetmap.h"
#include "reactive/simplesizehint.h"

#include "send.h"

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

    avg::Path rectToPath(avg::Rect const& r)
    {
        return makePathFromRect(r, 10.0f);
    }

    template <bool IsHorizontal>
    avg::Drawing drawSlider(avg::Vector2f size, widget::Theme const& theme,
            float amount, float handleSize, bool hover, bool isDown)
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

        avg::Path slider(rectToPath(getSliderRect<IsHorizontal>(size, amount,
                        handleSize)));

        return avg::Drawing()
            + makeShape(std::move(slider), btl::just(brush), btl::just(pen))
            ;
    }

    template <bool IsHorizontal>
    avg::Path getScrollLinePath(avg::Vector2f size)
    {

        if (IsHorizontal)
        {
            return avg::Path(avg::PathSpec()
                .start(0.0f, size[1] / 2.0f)
                .lineTo(size[0], size[1] / 2.0f)
                );
        }
        else
        {
            return avg::Path(avg::PathSpec()
                .start(size[0] / 2.0f, 0.0f)
                .lineTo(size[0] / 2.0f, size[1])
                );
        }
    }

    template <bool IsHorizontal>
    avg::Drawing drawScrollBar(avg::Vector2f size, widget::Theme const& theme,
            float amount, float handleSize, bool hover, bool isDown)
    {
        avg::Path line = getScrollLinePath<IsHorizontal>(size);

        avg::Pen pen(theme.getBackgroundHighlight(), 1.0f);

        return avg::Drawing()
            + makeShape(std::move(line), btl::none, btl::just(pen))
            + drawSlider<IsHorizontal>(size, theme, amount, handleSize,
                    hover, isDown)
            ;
    }

    template <bool IsHorizontal, typename T, typename U, typename V>
    auto scrollPointerDown(
            signal::InputHandle<btl::option<avg::Vector2f>> downHandle,
            Signal<avg::Vector2f, T> sizeSignal,
            SharedSignal<float, U> amountSignal,
            Signal<float, V> handleSizeSignal)
    {
        return signal::mapFunction(
            [downHandle=std::move(downHandle)]
            (avg::Vector2f size, float amount, float handleSize,
                PointerButtonEvent e) mutable
            {
                auto r = getSliderRect<IsHorizontal>(size, amount,
                        handleSize);

                if (e.button == 1 && r.contains(e.pos))
                    downHandle.set(btl::just(e.pos-r.getCenter()));

                return EventResult::possible;
            },
            std::move(sizeSignal),
            amountSignal,
            std::move(handleSizeSignal)
            );
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
} // anonymous namespace

template <bool IsHorizontal>
WidgetFactory scrollBar(
        signal::InputHandle<float> handle,
        SharedSignal<float> amount,
        SharedSignal<float> handleSize)
{
    return makeWidgetFactory()
        | bindWidgetMap<SizeTag>([=](auto size)
        {
            auto downOffset = signal::input<btl::option<avg::Vector2f>>(btl::none);
            auto hover = signal::input(false);
            auto isDown = signal::map(&btl::option<avg::Vector2f>::valid,
                    downOffset.signal);

            return
                onHover([handle=hover.handle](HoverEvent const& e) mutable
                    {
                        handle.set(e.hover);
                    })
                | onPointerDown(scrollPointerDown<IsHorizontal>(
                        downOffset.handle, size.clone(), amount, handleSize)
                    )
                | onPointerUp([handle=downOffset.handle]() mutable
                    {
                        handle.set(btl::none);
                        return EventResult::accept;
                    })
                | onPointerMove(signal::mapFunction(
                    [handle]
                    (btl::option<avg::Vector2f> downOffset,
                        avg::Vector2f size, float handleSize,
                        PointerMoveEvent const& e) mutable -> EventResult
                    {
                        if (!downOffset.valid())
                            return EventResult::possible;

                        if (handleSize == 1.0f)
                            return EventResult::possible;

                        int const axis = IsHorizontal ? 0 : 1;
                        float offset = (*downOffset)[axis];
                        float handleLen  = std::max(10.0f, handleSize * size[axis]);
                        float lineLen = size[axis] - handleLen;

                        float pos = e.pos[axis] - offset - handleLen / 2.0f;

                        float len = pos / lineLen;

                        handle.set(std::max(0.0f, std::min(len, 1.0f)));
                        return EventResult::accept;
                    }, downOffset.signal, size, handleSize))
                | onDraw<SizeTag, ThemeTag>(
                        drawScrollBar<IsHorizontal>,
                        amount,
                        handleSize,
                        hover.signal,
                        std::move(isDown)
                        )
                ;
        })
        | widget::margin(signal::constant(5.0f))
        | setSizeHint(getScrollBarSizeHint<IsHorizontal>())
        ;
}

WidgetFactory hScrollBar(
        signal::InputHandle<float> handle,
        Signal<float> amount,
        Signal<float> handleSize)
{
    return scrollBar<true>(std::move(handle), signal::share(std::move(amount)),
            signal::share(signal::map([](float v)
                    {
                        return std::min(1.0f, v);
                    }, std::move(handleSize))));
}

WidgetFactory vScrollBar(
        signal::InputHandle<float> handle,
        Signal<float> amount,
        Signal<float> handleSize)
{
    return scrollBar<false>(std::move(handle), signal::share(std::move(amount)),
            signal::share(signal::map([](float v)
                    {
                        return std::min(1.0f, v);
                    }, std::move(handleSize))));
}

} // namespace reactive::widget

