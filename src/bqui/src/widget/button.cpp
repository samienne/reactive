#include "bqui/widget/button.h"

#include "bqui/widget/label.h"

#include "bqui/modifier/margin.h"
#include "bqui/modifier/onpointerdown.h"
#include "bqui/modifier/onpointerup.h"
#include "bqui/modifier/onhover.h"
#include "bqui/modifier/onclick.h"
#include "bqui/modifier/ondraw.h"
#include "bqui/modifier/setwidgetintrospection.h"

#include "bqui/provider/providetheme.h"

#include "bqui/widget/introspection.h"
#include "bqui/widget/datavalue.h"

#include "bqui/shapes.h"

namespace bqui::widget
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

AnyWidget button(bq::signal::AnySignal<std::string> label,
        bq::signal::AnySignal<std::function<void()>> onClick)
{
    auto down = bq::signal::makeInput<bool>(false);
    auto hover = bq::signal::makeInput<bool>(false);

    auto sharedLabel = std::move(label).share();
    auto captionData = sharedLabel.map([](std::string text)
            {
                return DataValue(std::move(text));
            });

    return widget::label(sharedLabel)
        | modifier::margin(bq::signal::constant(5.0f))
        | modifier::makeWidgetModifier(
            [](auto widget, auto theme, auto downSignal, auto hoverSignal)
            {
                auto secondary = theme.map([](Theme const& theme)
                        {
                            return avg::Animated<avg::Color>(theme.getSecondary());
                        });

                return std::move(widget)
                    | modifier::onDrawBehind(drawButton, std::move(theme),
                            std::move(secondary), std::move(downSignal),
                            std::move(hoverSignal))
                    ;
            },
            provider::provideTheme(),
            std::move(down.signal),
            std::move(hover.signal)
            )
        | modifier::onPointerDown([handle=down.handle](auto&) mutable
            {
                handle.set(true);
                return EventResult::possible;
            })
        | modifier::onPointerUp([handle=down.handle](auto&) mutable
            {
                handle.set(false);
                return EventResult::possible;
            })
        | modifier::onHover([handle=hover.handle](HoverEvent const& e) mutable
            {
                handle.set(e.hover);
            })
        | modifier::onClick(1, std::move(onClick))
        | modifier::setRole("Button")
        | modifier::setData("text", std::move(captionData))
        | modifier::addCapability(widget::Capability::Clickable)
        ;
}

AnyWidget button(std::string label, bq::signal::AnySignal<std::function<void()>> onClick)
{
    return button(bq::signal::constant(std::move(label)), std::move(onClick));
}

}

