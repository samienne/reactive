#include "adder.h"
#include "spinner.h"
#include "curvevisualizer.h"

#include <bqui/modifier/setsize.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/drawkeyboardinputs.h>
#include <bqui/modifier/setminimumsize.h>
#include <bqui/modifier/settheme.h>
#include <bqui/modifier/focusgroup.h>
#include <bqui/modifier/frame.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/clip.h>
#include <bqui/modifier/onpointermove.h>
#include <bqui/modifier/onpointerdown.h>
#include <bqui/modifier/onhover.h>
#include <bqui/modifier/onclick.h>
#include <bqui/modifier/setgravity.h>
#include <bqui/modifier/transform.h>
#include <bqui/modifier/constraintsize.h>

#include <bqui/widget/scrollbar.h>
#include <bqui/widget/scrollview.h>
#include <bqui/widget/textedit.h>
#include <bqui/widget/button.h>
#include <bqui/widget/label.h>
#include <bqui/widget/builder.h>
#include <bqui/widget/filler.h>
#include <bqui/widget/uniformgrid.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/puresolver.h>

#include <bqui/shape/rectangle.h>

#include <bqui/modifier/setwidgetintrospection.h>

#include <bqui/widget/introspection.h>

#include <bqui/simplesizehint.h>
#include <bqui/keyboardinput.h>
#include <bqui/buildparams.h>
#include <bqui/send.h>
#include <bqui/window.h>
#include <bqui/app.h>
#include <bqui/withanimation.h>

#include <bq/signal/signal.h>
#include <bq/signal/signalcontext.h>

#include <avg/curve/curves.h>

#include <ase/vector.h>

#include <btl/future.h>

#include <iostream>
#include <string>
#include <vector>

using namespace bqui;

std::vector<std::pair<std::string, avg::Curve>> curves = {
    { "linear", avg::curve::linear },
    { "easeInCubic", avg::curve::easeInCubic },
    { "easeOutCubic", avg::curve::easeOutCubic },
    { "easeInOutCubic", avg::curve::easeInOutCubic },
    { "easeInElastic", avg::curve::easeInElastic },
    { "easeOutElastic", avg::curve::easeOutElastic },
    { "easeInOutElastic", avg::curve::easeInOutElastic },
    { "easeInQuad", avg::curve::easeInQuad },
    { "easeOutQuad", avg::curve::easeOutQuad },
    { "easeInOutQuad", avg::curve::easeInOutQuad },
    { "easeInBack", avg::curve::easeInBack },
    { "easeOutBack", avg::curve::easeOutBack },
    { "easeInOutBack", avg::curve::easeInOutBack },
    { "easeInBounce", avg::curve::easeInBounce },
    { "easeOutBounce", avg::curve::easeOutBounce },
    { "easeInOutBounce", avg::curve::easeInOutBounce },
};

void openSecondWindow()
{
    Window w = window(bq::signal::constant<std::string>("Second window"));

    app().addWindow(
            w,
            widget::button("Close me",
                    [w]()
                    {
                        w.close();
                    })
                | modifier::frame()
                | modifier::focusGroup());
}

namespace
{
    void printIntrospection(widget::Introspection const& node, int depth)
    {
        std::cout << std::string(depth * 2, ' ') << node.role;
        if (node.name)
            std::cout << " \"" << *node.name << "\"";

        if (!node.capabilities.empty())
        {
            std::cout << " [";
            for (size_t i = 0; i < node.capabilities.size(); ++i)
                std::cout << (i ? "," : "")
                    << widget::toString(node.capabilities[i]);
            std::cout << "]";
        }

        auto text = node.data.find("text");
        if (text != node.data.end())
            if (auto s = std::get_if<std::string>(&text->second.value))
                std::cout << " text=\"" << *s << "\"";

        auto size = node.obb.getSize();
        std::cout << " obb=" << size[0] << "x" << size[1];
        std::cout << "\n";

        for (auto const& child : node.children)
            printIntrospection(*child, depth + 1);
    }

    void printWidgetHierarchy(widget::AnyWidget widget, avg::Vector2f size)
    {
        // Introspection obbs are realised geometry, so drive a concrete size:
        // the tree is resolved as if laid out in a window of that size.
        auto introspection = std::move(widget)(BuildParams{})(
                    bq::signal::constant(size))
                .getIntrospection();
        auto data = bq::signal::makeSignalContext(std::move(introspection))
            .evaluate<0>().get<0>();

        std::cout << "Widget hierarchy (window " << size[0] << "x" << size[1]
            << "):\n";
        printIntrospection(data, 0);
        std::cout << std::endl;
    }
} // anonymous namespace

int main()
{
    auto textState = bq::signal::makeInput(widget::TextEditState{"Test123"});

    auto hScrollState = bq::signal::makeInput(0.5f);
    auto vScrollState = bq::signal::makeInput(0.5f);

    auto curveSelection = bq::signal::makeInput(0);
    auto curve = curveSelection.signal.map([](int i)
            {
                return curves.at(i).second;
            });

    auto curveName = curveSelection.signal.map([](int i) -> std::string
            {
                return curves.at(i).first;
            });

    auto m = bq::signal::makeInput<bool>(true);
    auto margin = m.signal.clone().map([](bool b) { return b ? 10.0f : 50.0f; });
    auto color = m.signal.clone().map([](bool b)
            {
                Theme theme;
                return b ? theme.getOrange() : theme.getGreen();
            });
    auto color2 = m.signal.clone().map([](bool b)
            {
                Theme theme;
                return b ? theme.getYellow() : theme.getBlue();
            });
    auto pen = color.clone().map([](auto color)
            {
                return avg::Pen(avg::Brush(color), 1.0f);
            });

    auto brush = color2.clone().map([](auto color)
            {
                return avg::Brush(color);
            });

    auto angle = bq::signal::constant(avg::infiniteAnimation(
                -0.1f, 0.1f, avg::curve::easeInOutCubic, 2.0f, avg::RepeatMode::reverse
                ));

    auto offset = bq::signal::constant(avg::infiniteAnimation(
                avg::Vector2f(-20,0),
                avg::Vector2f(20, 0),
                avg::curve::easeInOutCubic, 2.0f,
                avg::RepeatMode::reverse
                ));

    auto showTracked = bq::signal::makeInput(false);
    Window trackedWindow = window(
            bq::signal::constant<std::string>("Tracked window"));
    trackedWindow = std::move(trackedWindow).onClose(
            [opened = showTracked.handle]() mutable { opened.set(false); });

    auto openTracked = [trackedWindow, opened = showTracked.handle]() mutable
    {
        opened.set(true);

        Window w = trackedWindow;

        app().addWindow(
                trackedWindow,
                widget::button("Close me",
                        [w]() mutable
                        {
                            w.close();
                        })
                    | modifier::frame()
                    | modifier::focusGroup());
    };

    auto widgets = widget::hbox({
        widget::vbox({
            widget::button("Open another window",
                    []() { openSecondWindow(); })
                | modifier::setSizeHint({ 250, 50 }),
            widget::button(
                    showTracked.signal.map([](bool b) -> std::string
                        {
                            return b ? "Close tracked window"
                                : "Open tracked window";
                        }),
                    showTracked.signal.bindFirst(
                        [trackedWindow, openTracked](bool b) mutable
                        {
                            if (b)
                                trackedWindow.close();
                            else
                                openTracked();
                        }))
                | modifier::setSizeHint({ 250, 50 }),
            shape::rectangle()
                //.size(bq::signal::constant(avg::Vector2f(100, 100)))
                //.transform(bq::signal::constant(avg::translate(10, 20)))
                //.transform(avg::translate(10, 20))
                //.translate({10, 20})
                .translate(offset)
                //.translate(bq::signal::constant(avg::Vector2f(10, 20)))
                .rotate(angle)
                .fillAndStroke(std::move(brush), std::move(pen))
                | modifier::margin(std::move(margin))
                | modifier::onClick(0, m.signal.bindFirst(
                    [h=m.handle](bool b, ClickEvent const&) mutable
                    {
                        auto a = withAnimation(1.3f, avg::curve::easeOutBounce);
                        h.set(!b);
                    }))
                //| modifier::setSizeHint( {100.0f, 200.0} ),
                | modifier::setMinimumSize(avg::Vector2f{ 100.0f, 200.0f }),
            widget::label("Curves")
                | modifier::frame()
                | modifier::setName("curvesLabel"),
            curveVisualizer(std::move(curve)),
            widget::button(std::move(curveName), curveSelection.signal.bindFirst(
                        [handle=curveSelection.handle](int i) mutable
                        {
                            handle.set(static_cast<int>((i+1) % curves.size()));
                        }))
                | modifier::setGravity(avg::Vector2f{ 0.5f, 1.0f })
                | modifier::setSize(avg::Vector2f{ 150, 50 })
                | modifier::setSizeHint({ 300, 300 })
                | modifier::setName("nextCurveButton")
                | modifier::setRole("Button"),
            widget::vfiller()
        })
        , widget::vbox({
                widget::scrollView(
                        widget::uniformGrid(3, 3)
                        .cell(0, 0, 1, 1, spinner())
                        .cell(1, 1, 1, 1, spinner())
                        .cell(2, 2, 1, 1, spinner())
                        )
                , widget::label("AbcTest")
                    | modifier::frame()
                , widget::textEdit(textState.handle,
                        textState.signal.cast<widget::TextEditState>())
                , widget::vfiller()
                , widget::hScrollBar(hScrollState.handle, hScrollState.signal,
                        bq::signal::constant(0.0f))
                , widget::label(hScrollState.signal.toString())
                , widget::label(vScrollState.signal.toString())
                })
        , adder()
            | modifier::frame()
            | modifier::onHover([](bqui::HoverEvent const& e)
                    {
                        std::cout << "Hover: " << e.hover << std::endl;
                    })
            | modifier::setName("adder")
            | modifier::setRole("Adder")
        , widget::vScrollBar(vScrollState.handle, vScrollState.signal,
                bq::signal::constant(0.5f))
    });

    printWidgetHierarchy(widgets.clone(), avg::Vector2f(800.0f, 600.0f));
    printWidgetHierarchy(widgets.clone(), avg::Vector2f(400.0f, 300.0f));

    // Pure-solver flex demo. Resize the window and watch the fillers reflow:
    // the toolbar spacer, the form field, and the two equal-split spacers all
    // grow and shrink with the available width, while the fixed items hold.
    auto pureSolverDemo = widget::pureSolverRoot(widget::vbox({
        // Toolbar: three 80px items with a growing spacer before the last one.
        widget::hbox({
            widget::label("File") | modifier::frame()
                | modifier::fixedWidth(80.0f),
            widget::label("Edit") | modifier::frame()
                | modifier::fixedWidth(80.0f),
            widget::filler(),
            widget::label("Help") | modifier::frame()
                | modifier::fixedWidth(80.0f),
        }) | modifier::fixedHeight(40.0f),

        // Form row: a fixed 120px label then a field that fills the rest.
        widget::hbox({
            widget::label("Name:") | modifier::frame()
                | modifier::fixedWidth(120.0f),
            widget::filler() | modifier::frame(),
        }) | modifier::fixedHeight(40.0f),

        // Two fillers share the leftover width equally beside a fixed 60px box.
        widget::hbox({
            widget::label("60") | modifier::frame()
                | modifier::fixedWidth(60.0f),
            widget::filler() | modifier::frame(),
            widget::filler() | modifier::frame(),
        }) | modifier::fixedHeight(40.0f),

        // Absorb the remaining vertical space so the rows stay at the top.
        widget::filler(),
    }))
        | modifier::margin(10.0f)
        | modifier::frame()
        | modifier::focusGroup();

    return app()
        .addWindow(
                window(bq::signal::constant<std::string>("Pure-solver flex demo")),
                std::move(pureSolverDemo)
                // Restore the original demo UI by swapping the two lines above
                // for: std::move(widgets) | modifier::focusGroup()
                )
        .run();
}
