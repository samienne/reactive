#include "adder.h"
#include "reactive/widget/setsizehint.h"
#include "spinner.h"
#include "curvevisualizer.h"

#include <reactive/debug/drawkeyboardinputs.h>

#include <reactive/widget/settheme.h>
#include <reactive/widget/button.h>
#include <reactive/widget/label.h>
#include <reactive/widget/focusgroup.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/margin.h>
#include <reactive/widget/scrollbar.h>
#include <reactive/widget/scrollview.h>
#include <reactive/widget/clip.h>
#include <reactive/widget/onpointermove.h>
#include <reactive/widget/onpointerdown.h>
#include <reactive/widget/onhover.h>
#include <reactive/widget/builder.h>
#include <reactive/widget/onclick.h>

#include <reactive/shape/rectangle.h>

#include <reactive/filler.h>
#include <reactive/simplesizehint.h>
#include <reactive/keyboardinput.h>
#include <reactive/send.h>
#include <reactive/window.h>
#include <reactive/app.h>
#include <reactive/uniformgrid.h>
#include <reactive/hbox.h>
#include <reactive/vbox.h>
#include <reactive/withanimation.h>

#include <reactive/signal/signal.h>

#include <avg/curve/curves.h>

#include <ase/vector.h>

#include <btl/future.h>

#include <iostream>

using namespace reactive;

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
    auto textState = signal::makeInput(widget::TextEditState{"Test123"});

    auto hScrollState = signal::makeInput(0.5f);
    auto vScrollState = signal::makeInput(0.5f);

    auto curveSelection = signal::makeInput(0);
    auto curve = curveSelection.signal.map([](int i)
            {
                return curves.at(i).second;
            });

    auto curveName = curveSelection.signal.map([](int i) -> std::string
            {
                return curves.at(i).first;
            });

    auto m = signal::makeInput<bool>(true);
    auto margin = m.signal.clone().map([](bool b) { return b ? 10.0f : 50.0f; });
    auto color = m.signal.clone().map([](bool b)
            {
                reactive::widget::Theme theme;
                return b ? theme.getOrange() : theme.getGreen();
            });
    auto color2 = m.signal.clone().map([](bool b)
            {
                reactive::widget::Theme theme;
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

    auto widgets = hbox({
        vbox({
            shape::rectangle()
                .fillAndStroke(std::move(brush), std::move(pen))
                | widget::margin(std::move(margin))
                | widget::onClick(0, m.signal.bindToFunction([h=m.handle](bool b) mutable
                    {
                        auto a = withAnimation(1.3f, avg::curve::easeOutBounce);
                        h.set(!b);
                    }).cast<std::function<void()>>())
                | widget::setSizeHint(signal::constant(simpleSizeHint(100.0f, 200.0))),
            widget::label("Curves")
                | widget::frame(),
            curveVisualizer(std::move(curve)),
            widget::button(std::move(curveName), curveSelection.signal.bindToFunction(
                        [handle=curveSelection.handle](int i) mutable
                        {
                            handle.set(static_cast<int>((i+1) % curves.size()));
                        })),
            vfiller()
        })
        , vbox({
                widget::scrollView(
                        uniformGrid(3, 3)
                        .cell(0, 0, 1, 1, spinner())
                        .cell(1, 1, 1, 1, spinner())
                        .cell(2, 2, 1, 1, spinner())
                        )
                , widget::label("AbcTest")
                    | widget::frame()
                , widget::textEdit(textState.handle,
                        textState.signal.cast<widget::TextEditState>())
                , reactive::vfiller()
                , widget::hScrollBar(hScrollState.handle, hScrollState.signal,
                        signal::constant(0.0f))
                , widget::label(hScrollState.signal.toString())
                , widget::label(vScrollState.signal.toString())
                })
        , adder()
            | widget::frame()
            | widget::onHover([](reactive::HoverEvent const& e)
                    {
                        std::cout << "Hover: " << e.hover << std::endl;
                    })
        , widget::vScrollBar(vScrollState.handle, vScrollState.signal,
                signal::constant(0.5f))
    });

    return app()
        .windows({
                window(
                    signal::constant<std::string>("Test program"),
                    std::move(widgets)
                    //| debug::drawKeyboardInputs()
                    | widget::focusGroup()
                    )
                })
        .run();
}

