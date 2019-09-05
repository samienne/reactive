#include "widget/label.h"

#include "widget/binddrawcontext.h"
#include "widget/bindobb.h"
#include "widget/bindtheme.h"
#include "widget/margin.h"
#include "widget/theme.h"

#include "reactive/simplesizehint.h"

#include <reactive/signal/constant.h>

#include <reactive/rendering.h>

#include <avg/textextents.h>

#include <utf8/utf8.h>

#include <iostream>

namespace reactive { namespace widget {

WidgetFactory label(SharedSignal<std::string> text)
{
    auto draw = [](DrawContext const& drawContext, avg::Obb const& obb,
            widget::Theme const& theme,
            std::string const& text) -> avg::Drawing
    {
        auto const& size = obb.getSize();
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
                btl::just(avg::Brush(theme.getPrimary())),
                btl::none);

        return drawContext.drawing(std::move(textEntry));
    };

    auto getSizeHint = [](std::string const& text, widget::Theme const& theme)
        -> SizeHint
    {
        auto extents = theme.getFont().getTextExtents(
                utf8::asUtf8(text), theme.getTextHeight());

        return simpleSizeHint(extents.size[0], extents.size[1]);
    };

    auto theme = signal::input(Theme());

    return makeWidgetFactory()
        | makeWidgetMap()
            .provide(bindDrawContext(), bindObb(), bindTheme())
            .provideValues(text)
            .bindWidgetMap([draw]
            (auto drawContext, auto obb, auto theme, auto text) mutable
            {
                return onDraw(std::move(draw), std::move(drawContext),
                        std::move(obb), std::move(theme), std::move(text)
                        );
            })
        | trackTheme(theme.handle)
        | setSizeHint(signal::map(getSizeHint, text,
                    std::move(theme.signal)))
        | margin(signal::constant(5.0f))
        ;
}

}} // namespace

