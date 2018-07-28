#include "widget/scrollbar.h"

#include "widget/margin.h"

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
        return avg::Path(avg::PathSpec()
                .start(r.getBottomLeft())
                .lineTo(r.getBottomRight())
                .lineTo(r.getTopRight())
                .lineTo(r.getTopLeft())
                .close()
                );
    }

    template <bool IsHorizontal>
    avg::Drawing drawSlider(avg::Vector2f size, widget::Theme const& theme,
            float amount, float handleSize)
    {
        avg::Path slider(rectToPath(getSliderRect<IsHorizontal>(size, amount,
                        handleSize)));

        avg::Brush b(theme.getBackgroundHighlight());
        avg::Pen p(avg::Brush(theme.getEmphasized()));

        return avg::Drawing()
            + makeShape(std::move(slider), btl::just(b), btl::just(p))
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
            float amount, float handleSize)
    {
        avg::Path line = getScrollLinePath<IsHorizontal>(size);

        avg::Brush b(theme.getBackgroundHighlight());
        avg::Pen p(b, 1.0f);

        return avg::Drawing()
            + makeShape(std::move(line), btl::none, btl::just(p))
            + drawSlider<IsHorizontal>(size, theme, amount, handleSize)
            ;
    }

    template <typename F, typename T>
    std::function<F> toStdFunction(T&& t)
    {
        return std::function<F>(std::forward<T>(t));
    }

    template <bool IsHorizontal, typename T, typename U, typename V>
    auto scrollPointerDown(
            signal::InputHandle<btl::option<avg::Vector2f>> downHandle,
            Signal<avg::Vector2f, T> sizeSignal,
            SharedSignal<float, U> amountSignal,
            Signal<float, V> handleSizeSignal)
    {
        return
            signal::cast<std::function<void(ase::PointerButtonEvent const&)>>(
                    signal::mapFunction(
                        [downHandle=std::move(downHandle)]
                        (avg::Vector2f size, float amount, float handleSize,
                         PointerButtonEvent e) mutable
                        {
                            auto r = getSliderRect<IsHorizontal>(size, amount,
                                    handleSize);

                            if (e.button == 1 && r.contains(e.pos))
                                downHandle.set(btl::just(e.pos-r.getCenter()));
                        },
                        std::move(sizeSignal),
                        amountSignal,
                        std::move(handleSizeSignal))
                    );
    }

    template <bool IsHorizontal>
    auto getScrollBarSizeHint()
    {
        std::array<float, 3> main{{100, 200, 10000}};
        std::array<float, 3> aux{{30, 30, 0}};

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
    auto downOffset = signal::input<btl::option<avg::Vector2f>>(btl::none);
    auto size = signal::input(avg::Vector2f());

    return makeWidgetFactory()
        | trackSize(std::move(size.handle))
        | onPointerDown(scrollPointerDown<IsHorizontal>(
                    downOffset.handle, size.signal, amount, handleSize)
                )
        | onPointerUp([handle=downOffset.handle]() mutable { handle.set(btl::none); })
        | onPointerMove(signal::mapFunction(
                    [handle]
                    (btl::option<avg::Vector2f> downOffset,
                     avg::Vector2f size, float handleSize,
                     PointerMoveEvent const& e)
                    mutable
                    {
                        if (!downOffset.valid())
                            return;

                        int const axis = IsHorizontal ? 0 : 1;
                        float offset = (*downOffset)[axis];
                        float handleLen  = std::max(10.0f, handleSize * size[axis]);
                        float lineLen = size[axis] - handleLen;

                        float pos = e.pos[axis] - offset - handleLen / 2.0f;

                        float len = pos / lineLen;

                        handle.set(std::max(0.0f, std::min(len, 1.0f)));
                    }, downOffset.signal, size.signal, handleSize))
        | onDraw<SizeTag, ThemeTag>(drawScrollBar<IsHorizontal>, amount,
                handleSize)
        | widget::margin(signal::constant(10.0f))
        | setSizeHint(getScrollBarSizeHint<IsHorizontal>())
        ;
}

WidgetFactory hScrollBar(
        signal::InputHandle<float> handle,
        Signal<float> amount,
        Signal<float> handleSize)
{
    return scrollBar<true>(std::move(handle), signal::share(std::move(amount)),
            signal::share(std::move(handleSize)));
}

WidgetFactory vScrollBar(
        signal::InputHandle<float> handle,
        Signal<float> amount,
        Signal<float> handleSize)
{
    return scrollBar<false>(std::move(handle), signal::share(std::move(amount)),
            signal::share(std::move(handleSize)));
}

} // namespace reactive::widget

