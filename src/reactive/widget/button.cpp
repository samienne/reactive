#include "widget/button.h"

#include "widget/frame.h"
#include "widget/label.h"
#include "widget/onpointerdown.h"
#include "widget/onpointerup.h"
#include "widget/onhover.h"
#include "widget/onclick.h"
#include "widget/withparams.h"
#include "widget/settheme.h"

namespace reactive::widget
{

namespace
{
    avg::Drawing drawButton(
            avg::DrawContext const& drawContext,
            avg::Obb obb,
            Theme const& theme,
            avg::Color const& secondary,
            bool down,
            bool hover
            )
    {
        avg::Color fgColor(down ? theme.getEmphasized() : secondary);
        avg::Color bgColor = theme.getBackground();

        if (down)
            bgColor = secondary;
        else if (hover)
            bgColor = theme.getBackgroundHighlight();

        auto size = obb.getSize();

        auto pen = avg::Pen(fgColor);

        auto brush = avg::Brush(bgColor);

        auto shape =  makeShape(
                makeRoundedRect(drawContext.getResource(),
                    size[0] - 5.0f, size[1] - 5.0f, 10.0f),
                btl::just(brush),
                btl::just(pen));

        return avg::Transform()
            .translate(0.5f * size[0], 0.5f * size[1])
            * drawContext.drawing(shape);
    }
} // anonymous namespace

AnyWidget button(AnySignal<std::string> label,
        AnySignal<std::function<void()>> onClick)
{
    auto down = signal::input<bool>(false);
    auto hover = signal::input<bool>(false);

    return widget::label(std::move(label))
        | margin(signal::constant(5.0f))
        | widget::withParams<widget::ThemeTag>(
            [](auto widget, auto theme, auto downSignal, auto hoverSignal)
            {
                auto secondary = signal::map([](Theme const& theme)
                        {
                            return avg::Animated<avg::Color>(theme.getSecondary());
                        },
                        theme.clone()
                        );

                return std::move(widget)
                    | onDrawBehind(drawButton, std::move(theme), std::move(secondary),
                            std::move(downSignal), std::move(hoverSignal))
                    ;
            },
            std::move(down.signal),
            std::move(hover.signal)
            )
        | onPointerDown([handle=down.handle](auto&) mutable
            {
                handle.set(true);
                return EventResult::possible;
            })
        | onPointerUp([handle=down.handle](auto&) mutable
            {
                handle.set(false);
                return EventResult::possible;
            })
        | onHover([handle=hover.handle](HoverEvent const& e) mutable
            {
                handle.set(e.hover);
            })
        | widget::onClick(1, std::move(onClick))
        ;
}

AnyWidget button(std::string label, AnySignal<std::function<void()>> onClick)
{
    return button(signal::constant(std::move(label)), std::move(onClick));
}

} // namespace reactive::widget

