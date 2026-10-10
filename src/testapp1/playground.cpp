#include "playground.h"

#include <bqui/modifier/constraintsize.h>
#include <bqui/modifier/foreground.h>
#include <bqui/modifier/frame.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/ondraw.h>
#include <bqui/modifier/setgravity.h>

#include <bqui/widget/builder.h>
#include <bqui/widget/button.h>
#include <bqui/widget/filler.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/label.h>
#include <bqui/widget/scrollview.h>
#include <bqui/widget/textedit.h>
#include <bqui/widget/uniformgrid.h>
#include <bqui/widget/vbox.h>

#include <bqui/shape/rectangle.h>

#include <bqui/shapes.h>
#include <bqui/theme.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>

#include <avg/font.h>
#include <avg/rect.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace bqui;
using widget::AnyWidget;

namespace
{

avg::Color probeColor(int index)
{
    Theme theme;
    std::vector<avg::Color> const colors = {
        theme.getOrange(), theme.getBlue(), theme.getGreen(),
        theme.getMagenta(), theme.getCyan(), theme.getYellow(),
    };

    return colors.at(static_cast<size_t>(index) % colors.size());
}

avg::Drawing drawOutline(avg::DrawContext const& context, avg::Vector2f size,
        std::string const& name, avg::Color const& color)
{
    Theme theme;

    auto drawing = context.drawing();

    avg::Color const tint(color.getRed(), color.getGreen(), color.getBlue(),
            0.15f);
    drawing += avg::Shape(makePathFromRect(context.getResource(),
                avg::Rect(avg::Vector2f(0.5f, 0.5f),
                    avg::Vector2f(std::max(0.0f, size[0] - 1.0f),
                        std::max(0.0f, size[1] - 1.0f)))))
        .fillAndStroke(avg::Brush(tint), avg::Pen(avg::Brush(color), 1.0f));

    float const height = 12.0f;
    auto const& font = theme.getFont();
    float const descender = font.getDescender(height);

    auto text = [&](std::string const& str, float top)
    {
        return avg::TextEntry(font,
                avg::Transform()
                    .scale(height)
                    .translate(avg::Vector2f(4.0f,
                            size[1] - top - height + descender)),
                str,
                std::make_optional(avg::Brush(theme.getPrimary())),
                std::nullopt);
    };

    std::string const extent = std::to_string(std::lround(size[0])) + "x"
        + std::to_string(std::lround(size[1]));

    drawing += text(name, 3.0f);
    drawing += text(extent, 4.0f + height);

    return drawing;
}

// Overlays an outline of the widget's solved rect with its name and solved
// size; layout-transparent, so it wraps containers and fillers alike.
modifier::AnyWidgetModifier outlined(std::string name, int colorIndex)
{
    return modifier::foreground(widget::makeWidget()
            | modifier::onDraw(drawOutline,
                bq::signal::constant(std::move(name)),
                bq::signal::constant(probeColor(colorIndex))));
}

AnyWidget probe(std::string name, int colorIndex)
{
    return widget::makeWidget()
        | modifier::defaultSize(avg::Vector2f(90.0f, 36.0f))
        | outlined(std::move(name), colorIndex);
}

// A button that steps through @p names, and the index it is showing.
struct Cycle
{
    AnyWidget button;
    bq::signal::AnySignal<int> mode;
};

Cycle cycle(std::vector<std::string> names)
{
    auto mode = bq::signal::makeInput(0);
    int const count = static_cast<int>(names.size());

    auto button = widget::button(
            mode.signal.clone().map([names](int i) { return names.at(
                    static_cast<size_t>(i)); }),
            mode.signal.clone().bindFirst(
                [handle = mode.handle, count](int i) mutable
                {
                    handle.set((i + 1) % count);
                }));

    return { std::move(button), mode.signal.clone() };
}

// One child rebuilt from scratch whenever @p mode changes, so each mode can
// carry a different modifier chain.
bq::signal::ArraySignal<AnyWidget> swapped(bq::signal::AnySignal<int> mode,
        std::function<AnyWidget(int)> build)
{
    return bq::signal::forEach(
            std::move(mode).map([](int i) { return std::vector<int>{ i }; }),
            [](int i) { return i; },
            [build = std::move(build)](bq::signal::AnySignal<int>, int i)
            {
                return build(i);
            });
}

AnyWidget scenario(std::string title, AnyWidget content,
        std::optional<AnyWidget> control = std::nullopt)
{
    std::vector<bq::signal::ArraySignal<AnyWidget>> rows;
    rows.push_back(widget::hbox({
                widget::label(std::move(title)),
                widget::filler(),
            }));
    if (control)
    {
        rows.push_back(widget::hbox({
                    std::move(*control),
                    widget::filler(),
                }));
    }
    rows.push_back(std::move(content) | modifier::fill());

    return widget::vbox(bq::signal::ArraySignal<AnyWidget>(std::move(rows)))
        | modifier::margin(4.0f)
        | modifier::frame();
}

AnyWidget sidebarScenario()
{
    auto c = cycle({ "content: fill", "content: natural",
            "content: fixedWidth 200" });

    return scenario("Fixed sidebar",
            widget::hbox({
                probe("sidebar", 0) | modifier::fixedWidth(120.0f),
                swapped(c.mode, [](int i) -> AnyWidget
                    {
                        if (i == 0)
                            return probe("content | fill", 1) | modifier::fill();
                        if (i == 1)
                            return probe("content", 1);
                        return probe("content | fixed 200", 1)
                            | modifier::fixedWidth(200.0f);
                    }),
            }),
            std::move(c.button));
}

AnyWidget flexKindsScenario()
{
    auto c = cycle({ "3rd: grow(2)", "3rd: grow(3)", "3rd: fill",
            "3rd: natural" });

    return scenario("Natural, fill, grow",
            widget::hbox({
                probe("natural", 0),
                probe("fill", 1) | modifier::fill(),
                swapped(c.mode, [](int i) -> AnyWidget
                    {
                        if (i == 0)
                            return probe("grow(2)", 2) | modifier::grow(2.0f);
                        if (i == 1)
                            return probe("grow(3)", 2) | modifier::grow(3.0f);
                        if (i == 2)
                            return probe("fill", 2) | modifier::fill();
                        return probe("natural", 2);
                    }),
            }),
            std::move(c.button));
}

AnyWidget lastWriterScenario()
{
    return scenario("Last writer wins",
            widget::hbox({
                probe("fixed 100 | fill", 0)
                    | modifier::fixedWidth(100.0f)
                    | modifier::fill(),
                probe("fill | fixed 100", 1)
                    | modifier::fill()
                    | modifier::fixedWidth(100.0f),
            }));
}

AnyWidget boundsScenario()
{
    auto c = cycle({ "min 200 + max 80", "max 80 only", "min 200 only" });

    return scenario("Bounded fill",
            widget::hbox({
                swapped(c.mode.clone(), [](int i) -> AnyWidget
                    {
                        if (i == 1)
                            return probe("fill", 0) | modifier::fill();
                        return probe("fill | min 200", 0)
                            | modifier::fill()
                            | modifier::minWidth(200.0f);
                    }),
                swapped(c.mode.clone(), [](int i) -> AnyWidget
                    {
                        if (i == 2)
                            return probe("fill", 1) | modifier::fill();
                        return probe("fill | max 80", 1)
                            | modifier::fill()
                            | modifier::maxWidth(80.0f);
                    }),
                probe("fill", 2) | modifier::fill(),
            }),
            std::move(c.button));
}

AnyWidget cappedScenario()
{
    return scenario("Capped fill + filler",
            widget::hbox({
                probe("fill | max 150", 0)
                    | modifier::fill()
                    | modifier::maxWidth(150.0f),
                widget::filler() | outlined("filler", 1),
            }));
}

AnyWidget overflowScenario()
{
    auto c = cycle({ "width 80", "width 150", "width 250" });

    auto width = c.mode.clone().map([](int i)
            {
                float const widths[] = { 80.0f, 150.0f, 250.0f };
                return widths[i];
            }).share();

    return scenario("Fixed overflow",
            widget::hbox({
                probe("fixed", 0) | modifier::fixedWidth(width.clone()),
                probe("fixed", 1) | modifier::fixedWidth(width.clone()),
                probe("fixed", 2) | modifier::fixedWidth(width.clone()),
            }),
            std::move(c.button));
}

AnyWidget gridScenario()
{
    return scenario("Grid spans",
            widget::uniformGrid(3, 2)
                .cell(0, 1, 2, 1, probe("span 2x1 | growW", 0)
                    | modifier::growWidth())
                .cell(2, 0, 1, 2, probe("span 1x2 | growW+H", 1)
                    | modifier::growWidth()
                    | modifier::growHeight())
                .cell(0, 0, 1, 1, probe("natural", 2))
                .cell(1, 0, 1, 1, probe("fill", 3) | modifier::fill()));
}

AnyWidget nestedScenario()
{
    return scenario("Row in column",
            widget::vbox({
                probe("top", 0),
                widget::hbox({
                    probe("a", 1),
                    probe("b | fill", 2) | modifier::fill(),
                }) | outlined("row", 3),
                probe("bottom | fill", 4) | modifier::fill(),
            }));
}

AnyWidget scrollScenario()
{
    std::vector<bq::signal::ArraySignal<AnyWidget>> items;
    for (int i = 0; i < 8; ++i)
    {
        items.push_back(probe("item " + std::to_string(i), i)
                | modifier::fixedSize(avg::Vector2f(260.0f, 40.0f)));
    }

    return scenario("ScrollView slot",
            widget::hbox({
                probe("fixed 100", 0) | modifier::fixedWidth(100.0f),
                widget::scrollView(widget::vbox(
                        bq::signal::ArraySignal<AnyWidget>(std::move(items))))
                    | outlined("scrollView", 1),
            }));
}

AnyWidget weightedScenario()
{
    return scenario("Margin filler",
            widget::hbox({
                widget::filler()
                    | modifier::margin(20.0f)
                    | outlined("filler | margin 20", 0),
                probe("grow(1)", 1) | modifier::grow(1.0f),
                probe("grow(2)", 2) | modifier::grow(2.0f),
            }));
}

AnyWidget crossFillScenario()
{
    auto c = cycle({ "right: growHeight", "right: fill", "right: natural" });

    return scenario("Cross-axis fill",
            widget::hbox({
                widget::vbox({
                    widget::vfiller(),
                    probe("growHeight", 0) | modifier::growHeight(),
                    widget::vfiller(),
                }) | outlined("in vfillers", 1),
                swapped(c.mode, [](int i) -> AnyWidget
                    {
                        if (i == 0)
                            return probe("growHeight", 2)
                                | modifier::growHeight();
                        if (i == 1)
                            return probe("fill", 2) | modifier::fill();
                        return probe("natural", 2);
                    }),
            }),
            std::move(c.button));
}

AnyWidget panelScenario()
{
    auto formState = bq::signal::makeInput(widget::TextEditState{"Ada Lovelace"});

    Theme theme;

    auto barButton = [](std::string text) -> AnyWidget
    {
        return widget::button(text,
                        [text]() { std::cout << text << " clicked\n"; })
            | modifier::margin(4.0f);
    };

    auto swatch = [](avg::Color color) -> AnyWidget
    {
        return shape::rectangle().fill(color)
            | modifier::fixedSize(avg::Vector2f(48.0f, 48.0f))
            | modifier::margin(6.0f);
    };

    return scenario("Toolbar and form",
            widget::vbox({
                widget::hbox({
                    barButton("New"),
                    barButton("Open"),
                    barButton("Save"),
                    widget::filler(),
                    barButton("Help"),
                }),
                widget::hbox({
                    widget::label("Name:") | modifier::margin(6.0f),
                    AnyWidget(widget::textEdit(formState.handle,
                                formState.signal.cast<widget::TextEditState>()))
                        | modifier::margin(6.0f)
                        | modifier::fill(),
                }),
                widget::hbox({
                    widget::label("Palette:") | modifier::margin(6.0f),
                    swatch(theme.getOrange()),
                    swatch(theme.getBlue()),
                    swatch(theme.getGreen()),
                    widget::filler(),
                }),
                widget::filler(),
            }));
}

} // anonymous namespace

AnyWidget layoutPlayground()
{
    std::vector<AnyWidget> scenarios;
    scenarios.push_back(sidebarScenario());
    scenarios.push_back(flexKindsScenario());
    scenarios.push_back(lastWriterScenario());
    scenarios.push_back(boundsScenario());
    scenarios.push_back(cappedScenario());
    scenarios.push_back(overflowScenario());
    scenarios.push_back(gridScenario());
    scenarios.push_back(nestedScenario());
    scenarios.push_back(scrollScenario());
    scenarios.push_back(weightedScenario());
    scenarios.push_back(crossFillScenario());
    scenarios.push_back(panelScenario());

    unsigned int const columns = 3;
    unsigned int const rows = static_cast<unsigned int>(
            (scenarios.size() + columns - 1) / columns);

    auto grid = widget::uniformGrid(columns, rows);
    for (size_t i = 0; i < scenarios.size(); ++i)
    {
        auto const x = static_cast<unsigned int>(i % columns);
        auto const y = rows - 1 - static_cast<unsigned int>(i / columns);
        grid = std::move(grid).cell(x, y, 1, 1,
                std::move(scenarios[i])
                    | modifier::growWidth()
                    | modifier::growHeight()
                    // Pinned top-left, so a scenario whose content overflows
                    // grows away from its header rather than around its centre.
                    | modifier::setGravity(bq::signal::constant(
                            avg::Vector2f(0.0f, 1.0f))));
    }

    return std::move(grid);
}
