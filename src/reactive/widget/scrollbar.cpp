#include "widget/scrollbar.h"

#include "widget/margin.h"

#include "send.h"

namespace reactive::widget
{

namespace
{
    avg::Rect getSliderRect(avg::Vector2f size, float amount)
    {
        return avg::Rect(
                avg::Vector2f(size[0] * amount - 5.0f, size[1] / 2.0f - 10.0f),
                avg::Vector2f(10.0f, 20.0f)
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

    avg::Drawing drawSlider(avg::Vector2f size, widget::Theme const& theme,
            float amount)
    {
        avg::Path slider(rectToPath(getSliderRect(size, amount)));

        avg::Brush b(theme.getBackgroundHighlight());
        avg::Pen p(avg::Brush(theme.getEmphasized()));

        return avg::Drawing()
            + makeShape(std::move(slider), btl::just(b), btl::just(p))
            ;
    }

    avg::Drawing drawHScrollBar(avg::Vector2f size, widget::Theme const& theme,
            float amount)
    {
        avg::Path line(avg::PathSpec()
            .start(0.0f, size[1] / 2.0f)
            .lineTo(size[0], size[1] / 2.0f)
            );

        avg::Brush b(theme.getBackgroundHighlight());
        avg::Pen p(b, 1.0f);

        return avg::Drawing()
            + makeShape(std::move(line), btl::none, btl::just(p))
            + drawSlider(size, theme, amount)
            ;
    }

    template <typename F, typename T>
    std::function<F> toStdFunction(T&& t)
    {
        return std::function<F>(std::forward<T>(t));
    }

    template <typename T, typename U>
    auto hScrollPointerDown(
            signal::InputHandle<btl::option<avg::Vector2f>> downHandle,
            Signal<avg::Vector2f, T> sizeSignal,
            SharedSignal<float, U> amountSignal)
    {
        return
            signal::cast<std::function<void(ase::PointerButtonEvent const&)>>(
                    signal::mapFunction(
                        [downHandle=std::move(downHandle)]
                        (avg::Vector2f size, float amount, PointerButtonEvent e) mutable
                        {
                            auto r = getSliderRect(size, amount);

                            if (e.button == 1 && r.contains(e.pos))
                                downHandle.set(btl::just(e.pos-r.getCenter()));
                        }, std::move(sizeSignal), amountSignal));
    }
} // anonymous namespace

WidgetFactory hScrollBar(signal::InputHandle<float> handle, Signal<float> amount)
{
    auto downOffset = signal::input<btl::option<avg::Vector2f>>(btl::none);
    auto size = signal::input(avg::Vector2f());
    auto amountShared = signal::share(std::move(amount));

    return makeWidgetFactory()
        | trackSize(std::move(size.handle))
        | onPointerDown(hScrollPointerDown(downOffset.handle,
                    size.signal, amountShared))
        | onPointerUp([handle=downOffset.handle]() mutable { handle.set(btl::none); })
        | onPointerMove(signal::mapFunction(
                    [handle]
                    (btl::option<avg::Vector2f> downOffset,
                     avg::Vector2f size, PointerMoveEvent const& e)
                    mutable
                    {
                        if (!downOffset.valid())
                            return;

                        float offset = (*downOffset)[0];
                        float pos = e.pos[0] - offset;

                        handle.set(std::max(0.0f, std::min(pos / size[0], 1.0f)));
                    }, downOffset.signal, size.signal))
        | onDraw<SizeTag, ThemeTag>(drawHScrollBar, amountShared)
        | widget::margin(signal::constant(10.0f))
        | setSizeHint(signal::constant(simpleSizeHint(
                        {{100, 200, 10000}},
                        {{30, 30, 0}}
                        )))
        ;
}

} // namespace reactive::widget

