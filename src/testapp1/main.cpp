#include "adder.h"
#include "spinner.h"
#include "curvevisualizer.h"

#include <bqui/modifier/setsize.h>
#include <bqui/modifier/setsizehint.h>
#include <bqui/modifier/drawkeyboardinputs.h>
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
#include <bqui/send.h>
#include <bqui/window.h>
#include <bqui/app.h>
#include <bqui/withanimation.h>

#include <bq/signal/signal.h>

#include <avg/curve/curves.h>

#include <ase/vector.h>

#include <btl/future.h>

#include <iostream>

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

    auto widgets = widget::hbox({
        widget::vbox({
            shape::rectangle()
                .fillAndStroke(std::move(brush), std::move(pen))
                | modifier::margin(std::move(margin))
                | modifier::onClick(0, m.signal.bindToFunction([h=m.handle](bool b) mutable
                    {
                        auto a = withAnimation(1.3f, avg::curve::easeOutBounce);
                        h.set(!b);
                    }).cast<std::function<void()>>())
                | modifier::setSizeHint( {100.0f, 200.0} ),
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
        .windows({
                window(
                    bq::signal::constant<std::string>("Test program"),
                    std::move(widgets)
                    //| debug::drawKeyboardInputs()
                    | modifier::focusGroup()
                    )
                })
        .run();
}

