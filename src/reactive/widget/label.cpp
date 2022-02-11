#include "widget/label.h"

#include "widget/ondraw.h"
#include "widget/margin.h"
#include "widget/theme.h"
#include "widget/setsizehint.h"

#include "reactive/simplesizehint.h"

#include <reactive/signal/constant.h>

#include <avg/textextents.h>

#include <utf8/utf8.h>

#include <iostream>

namespace reactive::widget
{

AnyWidget label(AnySharedSignal<std::string> text)
{
    auto draw = [](avg::DrawContext const& drawContext, avg::Vector2f size,
            std::string const& text) -> avg::Drawing
    {
        widget::Theme theme;

        float height = theme.getTextHeight();
        auto& font = theme.getFont();
        auto te = font.getTextExtents(utf8::asUtf8(text), height);
        auto offset = ase::Vector2f(
                -te.bearing[0],
                (size[1] - height) * 0.5f + font.getDescender(height));

        auto textEntry = avg::TextEntry(
                font,
                avg::Transform()
                    .scale(height)
                    .translate(offset),
                text,
                std::make_optional(avg::Brush(theme.getPrimary())),
                std::nullopt);

        return drawContext.drawing(std::move(textEntry));
    };

    auto getSizeHint = [](std::string const& text, widget::Theme const& theme)
        -> SizeHint
    {
        auto extents = theme.getFont().getTextExtents(
                utf8::asUtf8(text), theme.getTextHeight());

        return simpleSizeHint(extents.size[0], extents.size[1]);
    };

    return makeWidget()
        | onDraw(draw, text)
        | setSizeHint(signal::map(getSizeHint, text,
                    signal::constant(Theme())))
        | margin(signal::constant(5.0f))
        ;
}

AnyWidget label(std::string const& text)
{
    return label(signal::constant(std::move(text)));
}

} // namespace reactive::widget

