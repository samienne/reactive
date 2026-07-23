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

#include <bqui/simplesizehint.h>
#include <bqui/keyboardinput.h>
#include <bqui/window.h>
#include <bqui/app.h>
#include <bqui/withanimation.h>

#include <bq/signal/signal.h>

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

// The window's own close button captures the owning Window and closes it. The
// window is a small handle that lives in the app-owned widget, so capturing it
// closes no cycle: the app holds the widget, the widget holds the window, and
// the window points back at the app only weakly.
void openSecondWindow()
{
    Window w = window(bq::signal::constant<std::string>("Second window"));

    app().addWindow(
            w,
            widget::button("Close me",
                    bq::signal::constant(std::function<void()>(
                            [w]()
                            {
                                w.close();
                            })))
                | modifier::frame()
                | modifier::focusGroup());
}

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

    // A tracked window the app owns, toggled by a button and closable by its
    // own close button. 'showTracked' tracks whether it is open so the button
    // reads the right label. The window is a persistent handle: its data — and
    // its title-bar onClose that flips the flag — outlive each close, so a
    // re-open supplies only a fresh widget. onClose is set once, here, rather
    // than per-open, so it is not appended anew on every open.
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
                widget::button("Close me", bq::signal::constant(
                        std::function<void()>([w]() mutable
                            {
                                w.close();
                            })))
                    | modifier::frame()
                    | modifier::focusGroup());
    };

    auto widgets = widget::hbox({
        widget::vbox({
            // Every press opens another window. The app owns them, so each one
            // closes by its own means and the app runs until none is left.
            widget::button("Open another window",
                    bq::signal::constant(std::function<void()>(
                            []() { openSecondWindow(); })))
                | modifier::setSizeHint({ 250, 50 }),
            widget::button(
                    showTracked.signal.map([](bool b) -> std::string
                        {
                            return b ? "Close tracked window"
                                : "Open tracked window";
                        }),
                    showTracked.signal.bindToFunction(
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
                | modifier::onClick(0, m.signal.bindToFunction([h=m.handle](bool b) mutable
                    {
                        auto a = withAnimation(1.3f, avg::curve::easeOutBounce);
                        h.set(!b);
                    }).cast<std::function<void()>>())
                //| modifier::setSizeHint( {100.0f, 200.0} ),
                | modifier::setMinimumSize({ 100.0f, 200.0f }),
            widget::label("Curves")
                | modifier::frame(),
            curveVisualizer(std::move(curve)),
            widget::button(std::move(curveName), curveSelection.signal.bindToFunction(
                        [handle=curveSelection.handle](int i) mutable
                        {
                            handle.set(static_cast<int>((i+1) % curves.size()));
                        }))
                | modifier::setGravity({ 0.5f, 1.0f })
                | modifier::setSize({ 150, 50 })
                | modifier::setSizeHint({ 300, 300 }),
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
        , widget::vScrollBar(vScrollState.handle, vScrollState.signal,
                bq::signal::constant(0.5f))
    });

    return app()
        .addWindow(
                window(bq::signal::constant<std::string>("Test program")),
                std::move(widgets)
                //| debug::drawKeyboardInputs()
                | modifier::focusGroup()
                )
        .run();
}
