#include "adder.h"
#include "spinner.h"
#include "testwidget.h"

#include <reactive/keyboardinput.h>

#include <reactive/widget/scrollbar.h>
#include <reactive/widget/scrollview.h>
#include <reactive/widget/clip.h>
#include <reactive/signal/map.h>
#include <reactive/widget/focusgroup.h>
#include <reactive/debug/drawkeyboardinputs.h>
#include <reactive/stack.h>
#include <reactive/send.h>
#include <reactive/window.h>
#include <reactive/app.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/label.h>
#include <reactive/uniformgrid.h>
#include <reactive/hbox.h>
#include <reactive/vbox.h>
#include <reactive/widgetfactory.h>
#include <reactive/signal/tostring.h>
#include <reactive/signal/constant.h>
#include <reactive/signal/input.h>
#include <reactive/signal.h>
#include <reactive/filler.h>

#include <ase/vector.h>

#include <btl/future.h>

#include <iostream>
#include <chrono>

using namespace reactive;

int main()
{
    auto textState = signal::input(widget::TextEditState{"Test123"});

    auto scrollState = signal::input(0.5f);

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
                            })
                    | widget::onPointerDown([](reactive::PointerButtonEvent const&)
                            {
                                std::cout << "down" << std::endl;
                            })
                , widget::label(signal::constant<std::string>("AbcTest"))
                    | widget::frame()
                , widget::textEdit(textState.handle,
                        signal::cast<widget::TextEditState>(textState.signal))
                , reactive::vfiller()
                , widget::hScrollBar(scrollState.handle, scrollState.signal)
                , widget::label(signal::toString(scrollState.signal))
                })
        , adder()
            | widget::frame()
            | widget::onHover([](reactive::HoverEvent const& e)
                    {
                        std::cout << "Hover: " << e.hover << std::endl;
                    })
    });

    auto running = signal::input(true);

    return App()
        .windows({
                window(
                    signal::constant<std::string>("Test program"),
                    std::move(widgets)
                    //| debug::drawKeyboardInputs()
                    | widget::focusGroup()
                    )
                .onClose(send(false, running.handle))
                })
        //.run(running.signal);
        .run();
}

