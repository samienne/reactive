#include "widget/button.h"

#include "widget/frame.h"
#include "widget/label.h"

namespace reactive::widget
{

namespace
{
    avg::Drawing drawButton(avg::Vector2f size, widget::Theme const& theme,
            bool hover, bool down)
    {
        avg::Color bgColor = theme.getBackground();
        avg::Color fgColor = theme.getSecondary();

        if (down)
        {
            fgColor = theme.getEmphasized();
            bgColor = theme.getSecondary();
        }
        else if (hover)
        {
            bgColor = theme.getBackgroundHighlight();
        }

        auto pen = avg::Pen(fgColor);

        auto brush = avg::Brush(bgColor);

        auto shape =  makeShape(
                makeRoundedRect(size[0] - 5.0f, size[1] - 5.0f, 10.0f),
                btl::just(brush),
                btl::just(pen));

        return avg::Transform()
            .translate(0.5f * size[0], 0.5f * size[1])
            * avg::Drawing(shape);
    }
} // anonymous namespace

WidgetFactory button(Signal<std::string> label,
        Signal<std::function<void()>> onClick)
{
    auto down = signal::input<bool>(false);
    auto hover = signal::input<bool>(false);

    return widget::label(std::move(label))
                    | margin(signal::constant(5.0f))
                    | onDrawBehind<SizeTag, ThemeTag>(
                                &drawButton,
                                std::move(hover.signal),
                                std::move(down.signal)
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

WidgetFactory button(std::string label, Signal<std::function<void()>> onClick)
{
    return button(signal::constant(std::move(label)), std::move(onClick));
}

} // namespace reactive::widget
