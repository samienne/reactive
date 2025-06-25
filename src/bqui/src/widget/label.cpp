#include "bqui/widget/label.h"

#include "bqui/modifier/ondraw.h"
#include "bqui/modifier/margin.h"
#include "bqui/modifier/setsizehint.h"

#include "bqui/provider/providetheme.h"

#include "bqui/simplesizehint.h"
#include "bqui/theme.h"

#include <avg/textextents.h>

#include <utf8/utf8.h>

namespace bqui::widget
{

namespace
{

auto drawLabel(avg::DrawContext const& drawContext, avg::Vector2f size,
            std::string const& text)
{
    Theme theme;

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

SizeHint makeLabelSizeHint(std::string const& text, Theme const& theme)
{
    auto extents = theme.getFont().getTextExtents(
            utf8::asUtf8(text), theme.getTextHeight());

    return simpleSizeHint(extents.size[0], extents.size[1]);
}

auto makeLabel(bq::signal::AnySignal<Theme> theme,
        bq::signal::AnySignal<std::string> text)
{
    return makeWidget()
        | modifier::onDraw(drawLabel, text)
        | modifier::setSizeHint(merge(text, std::move(theme)).map(makeLabelSizeHint))
        | modifier::margin(bq::signal::constant(5.0f))
        ;
}

} // anonymous namespace

AnyWidget label(bq::signal::AnySignal<std::string> text)
{
    return makeWidget(makeLabel, provider::provideTheme(), std::move(text));
}

AnyWidget label(std::string const& text)
{
    return label(bq::signal::constant(std::move(text)));
}

}

