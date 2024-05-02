#include "widget/label.h"

#include "widget/providetheme.h"
#include "widget/ondraw.h"
#include "widget/margin.h"
#include "widget/theme.h"
#include "widget/setsizehint.h"

#include "reactive/simplesizehint.h"

#include <avg/textextents.h>

#include <utf8/utf8.h>

namespace reactive::widget
{

namespace
{

auto drawLabel(avg::DrawContext const& drawContext, avg::Vector2f size,
            std::string const& text)
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
}

SizeHint makeLabelSizeHint(std::string const& text, widget::Theme const& theme)
{
    auto extents = theme.getFont().getTextExtents(
            utf8::asUtf8(text), theme.getTextHeight());

    return simpleSizeHint(extents.size[0], extents.size[1]);
}

auto makeLabel(signal2::AnySignal<widget::Theme> theme,
        signal2::AnySignal<std::string> text)
{
    return makeWidget()
        | onDraw(drawLabel, text)
        | setSizeHint(merge(text, std::move(theme)).map(makeLabelSizeHint))
        | margin(signal2::constant(5.0f))
        ;
}

} // anonymous namespace

AnyWidget label(signal2::AnySignal<std::string> text)
{
    return makeWidget(makeLabel, provideTheme(), std::move(text));
}

AnyWidget label(std::string const& text)
{
    return label(signal2::constant(std::move(text)));
}

} // namespace reactive::widget

