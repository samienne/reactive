#include "adder.h"
#include "spinner.h"
#include "testwidget.h"

#include <reactive/debug/drawkeyboardinputs.h>

#include <reactive/widget/label.h>
#include <reactive/widget/focusgroup.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/scrollbar.h>
#include <reactive/widget/scrollview.h>
#include <reactive/widget/clip.h>
#include <reactive/widget/onpointermove.h>
#include <reactive/widget/onpointerdown.h>
#include <reactive/widget/onhover.h>

#include <reactive/filler.h>
#include <reactive/keyboardinput.h>
#include <reactive/send.h>
#include <reactive/window.h>
#include <reactive/app.h>
#include <reactive/uniformgrid.h>
#include <reactive/hbox.h>
#include <reactive/vbox.h>
#include <reactive/widgetfactory.h>

#include <reactive/signal/tostring.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/input.h>
#include <reactive/signal/signal.h>

#include <reactive/signal/map.h>

#include <ase/vector.h>

#include <btl/future.h>

#include <iostream>
#include <chrono>

using namespace reactive;

int main()
{
    auto textState = signal::input(widget::TextEditState{"Test123"});

    auto hScrollState = signal::input(0.5f);
    auto vScrollState = signal::input(0.5f);

    auto widgets = hbox({
        widget::label(signal::constant<std::string>("TestTest"))
            | widget::frame()
        , vbox({
                widget::scrollView(
                        uniformGrid(3, 3)
                        .cell(0, 0, 1, 1, makeSpinner())
                        .cell(1, 1, 1, 1, makeSpinner())
                        .cell(2, 2, 1, 1, makeSpinner())
                        )
                , makeSpinner()
                    | widget::frame()
                    | widget::onPointerMove([](reactive::PointerMoveEvent const& e)
                            {
                                std::cout << "MoveEvent: " << e.rel << " " << e.pos
                                    << ", " << e.buttons[0] << " " << e.buttons[1]
                                    << " " << e.buttons[2] << " " << e.buttons[3]
                                    << " hover: " << e.hover
                                    << std::endl;

                                return EventResult::possible;
                            })
                    | widget::onPointerDown([](reactive::PointerButtonEvent const&)
                            {
                                std::cout << "down" << std::endl;
                                return EventResult::possible;
                            })
                , widget::label(signal::constant<std::string>("AbcTest"))
                    | widget::frame()
                , widget::textEdit(textState.handle,
                        signal::cast<widget::TextEditState>(textState.signal))
                , reactive::vfiller()
                , widget::hScrollBar(hScrollState.handle, hScrollState.signal,
                        signal::constant(0.0f))
                , widget::label(signal::toString(hScrollState.signal))
                , widget::label(signal::toString(vScrollState.signal))
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

