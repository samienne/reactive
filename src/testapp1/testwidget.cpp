#include "testwidget.h"

#include <bqui/modifier/onclick.h>
#include <bqui/modifier/ondraw.h>
#include <bqui/modifier/onkeyevent.h>
#include <bqui/modifier/setsizehint.h>

#include <bqui/widget/builder.h>

#include <bqui/shapes.h>
#include <bqui/send.h>
#include <bqui/simplesizehint.h>

#include <bq/signal/signal.h>

#include <bq/stream/iterate.h>
#include <bq/stream/pipe.h>

#include <avg/font.h>

#include <ase/stringify.h>
#include <ase/vector.h>

using namespace bqui;

namespace
{
    auto drawTestWidget = [](avg::DrawContext const& drawContext,
            avg::Obb const& obb, bool state, std::string const& str)
        -> avg::Drawing
    {
        Theme theme;

        auto size = obb.getSize();

        std::cout << "draw() -> " << size[0] << ", " << size[1] << std::endl;
        avg::Pen pen(theme.getEmphasized(), 8.0f);
        avg::Brush brush(
                state ?
                theme.getCyan() :
                theme.getBackgroundHighlight());
        avg::Brush brush2(avg::Color(0.4f, 0.3f, 0.0f));

        auto rect = makeRect(drawContext.getResource(),
                size[0] - 50.0f, 200.0f);
        auto shape = avg::Shape(rect).fillAndStroke(brush, pen);

        auto text = avg::TextEntry(theme.getFont(), avg::Transform(
                    avg::Vector2f(-70.0f, 0.0f), 20.0f),
                str, std::make_optional(brush2), std::nullopt);

        return (std::move(shape) + text)
            .transform(avg::translate(0.5f*size[0], 116.0f));
    };
} // anonymous namespace

widget::AnyWidget makeTestWidget()
{
    using namespace bqui::widget;

    auto p = bq::stream::pipe<int>();

    auto state = bq::stream::iterate(
            [](bool v, int) -> bool
            {
                return !v;
            },
            false,
            std::move(p.stream));

    auto p2 = bq::stream::pipe<KeyEvent>();
    auto textState = bq::stream::iterate(
            [](std::string const&, KeyEvent const& e) -> std::string
            {
                return ase::toString(e.getKey());
            },
            std::string(), std::move(p2.stream));

    auto focus = bq::signal::makeInput(false);

    return widget::makeWidget()
        | modifier::onDraw(drawTestWidget, std::move(state), std::move(textState))
        | modifier::onClick(1, send(1, p.handle))
        | modifier::onClick(1, send(true, focus.handle))
        | modifier::onKeyEvent(sendKeysTo(p2.handle))
        | modifier::setSizeHint(bq::signal::constant(simpleSizeHint(
                    {{200.0f, 400.0f, 10000.0f}},
                    {{50.0f, 150.0f, 10000.0f}})))
    ;
}

