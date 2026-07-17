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

// The window's own close button holds the handle, not the window: a window
// captured inside its own widget would own the widget that owns it. The
// handle is minted first for exactly this.
Window makeSecondWindow()
{
    WindowHandle handle;

    return window(
            bq::signal::constant<std::string>("Second window"),
            widget::button("Close me",
                    bq::signal::constant(std::function<void()>(
                            [handle]()
                            {
                                handle.close();
                            })))
                | modifier::frame()
                | modifier::focusGroup(),
            handle);
}

namespace
{
    char const* capabilityName(widget::Capability cap)
    {
        switch (cap)
        {
            case widget::Capability::Clickable: return "Clickable";
            case widget::Capability::Editable: return "Editable";
            case widget::Capability::Focusable: return "Focusable";
            case widget::Capability::Scrollable: return "Scrollable";
        }
        return "?";
    }

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
                    << capabilityName(node.capabilities[i]);
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
            printIntrospection(child, depth + 1);
    }

    void printWidgetHierarchy(widget::AnyWidget widget)
    {
        auto introspection = std::move(widget)(BuildParams{}).getIntrospection();
        auto data = bq::signal::makeSignalContext(std::move(introspection))
            .evaluate<0>().get<0>();

        std::cout << "Widget hierarchy:\n";
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

    // The tracked window follows an input of the caller's own rather than the
    // app's collection, which is the other way to have windows: the array is
    // the source of truth, so every way of closing the window takes its key out
    // of the input.
    auto showTracked = bq::signal::makeInput(false);

    auto trackedWindows = bq::signal::forEach(
            showTracked.signal.map([](bool b)
                {
                    std::vector<std::string> names;
                    if (b)
                        names.push_back("Tracked window");
                    return names;
                }),
            [](std::string const& name) { return name; },
            [handle = showTracked.handle]
            (bq::signal::AnySignal<std::string> name)
            {
                return window(std::move(name),
                        widget::button("Close me", bq::signal::constant(
                                    std::function<void()>(send(false, handle))))
                        | modifier::frame()
                        | modifier::focusGroup()
                        )
                    .onClose(send(false, handle));
            });

    auto widgets = widget::hbox({
        widget::vbox({
            // Every press opens another window. The app owns them, so each one
            // closes by its own means and the app runs until none is left.
            widget::button("Open another window",
                    bq::signal::constant(std::function<void()>(
                            []() { app().addWindow(makeSecondWindow()); })))
                | modifier::setSizeHint({ 250, 50 }),
            widget::button(
                    showTracked.signal.map([](bool b) -> std::string
                        {
                            return b ? "Close tracked window"
                                : "Open tracked window";
                        }),
                    showTracked.signal.bindToFunction(
                        [handle = showTracked.handle](bool b) mutable
                        {
                            handle.set(!b);
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
                | modifier::onClick(0, m.signal.bindToFunction([h=m.handle](bool b) mutable
                    {
                        auto a = withAnimation(1.3f, avg::curve::easeOutBounce);
                        h.set(!b);
                    }).cast<std::function<void()>>())
                //| modifier::setSizeHint( {100.0f, 200.0} ),
                | modifier::setMinimumSize({ 100.0f, 200.0f }),
            widget::label("Curves")
                | modifier::frame()
                | modifier::setName("curvesLabel"),
            curveVisualizer(std::move(curve)),
            widget::button(std::move(curveName), curveSelection.signal.bindToFunction(
                        [handle=curveSelection.handle](int i) mutable
                        {
                            handle.set(static_cast<int>((i+1) % curves.size()));
                        }))
                | modifier::setGravity({ 0.5f, 1.0f })
                | modifier::setSize({ 150, 50 })
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

    printWidgetHierarchy(widgets.clone());

    return app()
        .addWindow(window(
                    bq::signal::constant<std::string>("Test program"),
                    std::move(widgets)
                    //| debug::drawKeyboardInputs()
                    | modifier::focusGroup()
                    ))
        .addWindowArray(std::move(trackedWindows))
        .run();
}
