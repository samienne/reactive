#include "widget/button.h"

#include "widget/margin.h"
#include "widget/label.h"
#include "widget/onpointerdown.h"
#include "widget/onpointerup.h"
#include "widget/onhover.h"
#include "widget/onclick.h"
#include "widget/providetheme.h"
#include "widget/ondraw.h"

#include "reactive/shapes.h"

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
        auto path =  makeRoundedRect(drawContext.getResource(),
                size[0] - 5.0f, size[1] - 5.0f, 10.0f);

        auto transform = avg::Transform()
            .translate(0.5f * size[0], 0.5f * size[1]);

        return transform * avg::Shape(std::move(path))
            .fillAndStroke(brush, pen);
    }
} // anonymous namespace

AnyWidget button(signal::AnySignal<std::string> label,
        signal::AnySignal<std::function<void()>> onClick)
{
    auto down = signal::makeInput<bool>(false);
    auto hover = signal::makeInput<bool>(false);

    return widget::label(std::move(label))
        | margin(signal::constant(5.0f))
        | widget::makeWidgetModifier(
            [](auto widget, auto theme, auto downSignal, auto hoverSignal)
            {
                auto secondary = theme.map([](Theme const& theme)
                        {
                            return avg::Animated<avg::Color>(theme.getSecondary());
                        });

                return std::move(widget)
                    | onDrawBehind(drawButton, std::move(theme), std::move(secondary),
                            std::move(downSignal), std::move(hoverSignal))
                    ;
            },
            provideTheme(),
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

AnyWidget button(std::string label, signal::AnySignal<std::function<void()>> onClick)
{
    return button(signal::constant(std::move(label)), std::move(onClick));
}

} // namespace reactive::widget

