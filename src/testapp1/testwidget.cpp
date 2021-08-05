#include "testwidget.h"

#include <reactive/widget/onclick.h>
#include <reactive/widget/ondrawcustom.h>
#include <reactive/widget/bindsize.h>
#include <reactive/widget/bindtheme.h>
#include <reactive/widget/onkeyevent.h>
#include <reactive/widget/widgettransformer.h>

#include <reactive/shapes.h>
#include <reactive/send.h>

#include <reactive/widgetfactory.h>
#include <reactive/simplesizehint.h>

#include <reactive/signal/droprepeats.h>
#include <reactive/signal/constant.h>

#include <reactive/stream/iterate.h>
#include <reactive/stream/pipe.h>

#include <avg/font.h>

#include <ase/stringify.h>
#include <ase/vector.h>

using namespace reactive;

namespace
{
    auto drawTestWidget = [](avg::DrawContext const& drawContext,
            avg::Obb const& obb, widget::Theme const& theme,
            bool state, std::string const& str)
        -> avg::Drawing
    {
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
        auto shape = makeShape(rect, btl::just(brush), btl::just(pen));

        auto text = avg::TextEntry(theme.getFont(), avg::Transform(
                    avg::Vector2f(-70.0f, 0.0f), 20.0f),
                str, btl::just(brush2), btl::none);

        return (drawContext.drawing(std::move(shape)) + text)
            .transform(avg::translate(0.5f*size[0], 116.0f));
    };
} // anonymous namespace

WidgetFactory makeTestWidget()
{
    using namespace reactive::widget;

    auto p = stream::pipe<int>();

    auto state = stream::iterate(
            [](bool v, int) -> bool
            {
                return !v;
            },
            false,
            std::move(p.stream));

    auto p2 = stream::pipe<KeyEvent>();
    auto textState = stream::iterate(
            [](std::string const&, KeyEvent const& e) -> std::string
            {
                return ase::toString(e.getKey());
            },
            std::string(), std::move(p2.stream));

    auto focus = signal::input(false);

    return makeWidgetFactory()
        | makeWidgetTransformer()
        .compose(bindTheme())
        .values(std::move(state), std::move(textState))
        .bind(onDrawCustom(drawTestWidget))
        | widget::onClick(1, send(1, p.handle))
        | widget::onClick(1, send(true, focus.handle))
        | widget::onKeyEvent(sendKeysTo(p2.handle))
        | setSizeHint(signal::constant(simpleSizeHint(
                    {{200.0f, 400.0f, 10000.0f}},
                    {{50.0f, 150.0f, 10000.0f}})))
    ;
}

