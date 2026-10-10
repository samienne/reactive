#include "bqui/widget/label.h"

#include "bqui/modifier/constraintsize.h"
#include "bqui/modifier/ondraw.h"
#include "bqui/modifier/margin.h"
#include "bqui/modifier/setwidgetintrospection.h"
#include "modifier/pureconstraint.h"

#include "bqui/widget/datavalue.h"

#include "bqui/provider/providetheme.h"

#include "bqui/theme.h"

#include <avg/textextents.h>

#include <utf8/utf8.h>

namespace bqui::widget
{

namespace
{

// Where the text's baseline sits, measured up from the bottom of a box
// @p boxHeight tall: the text height centred in the box, lowered by the font's
// (negative) descender.
float baselineAboveBottom(float boxHeight, Theme const& theme)
{
    float height = theme.getTextHeight();
    return (boxHeight - height) * 0.5f
        + theme.getFont().getDescender(height);
}

// The same baseline as an anchor, top down: boxHeight minus
// baselineAboveBottom(), which is half the box height plus a constant.
Anchor labelBaseline(Theme const& theme)
{
    float height = theme.getTextHeight();
    return Anchor{ 0.5f,
        0.5f * height - theme.getFont().getDescender(height) };
}

auto drawLabel(avg::DrawContext const& drawContext, avg::Vector2f size,
            std::string const& text, Theme const& theme)
{
    float height = theme.getTextHeight();
    auto& font = theme.getFont();
    auto te = font.getTextExtents(utf8::asUtf8(text), height);
    auto offset = ase::Vector2f(
            -te.bearing[0],
            baselineAboveBottom(size[1], theme));

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

avg::TextExtents measureLabel(std::string const& text, Theme const& theme)
{
    return theme.getFont().getTextExtents(
            utf8::asUtf8(text), theme.getTextHeight());
}

auto makeLabel(bq::signal::AnySignal<Theme> theme,
        bq::signal::AnySignal<std::string> text)
{
    auto textData = text.map([](std::string text)
            {
                return DataValue(std::move(text));
            });

    auto sharedTheme = std::move(theme).share();
    auto extents = merge(text, sharedTheme.clone()).map(measureLabel).share();

    return makeWidget()
        | modifier::onDraw(drawLabel, text, sharedTheme.clone())
        | modifier::defaultSize(extents.clone().map(
                    [](avg::TextExtents const& e) { return e.size; }))
        | modifier::detail::pureAnchorModifier(
                modifier::detail::PureAxis::vertical, baselineAnchor,
                sharedTheme.clone().map(labelBaseline))
        | modifier::margin(bq::signal::constant(5.0f))
        | modifier::setRole("Label")
        | modifier::setData("text", std::move(textData))
        ;
}

} // anonymous namespace

AnyWidget label(bq::signal::AnySignal<std::string> text)
{
    return makeWidget(makeLabel, provider::provideTheme(), std::move(text));
}

}

