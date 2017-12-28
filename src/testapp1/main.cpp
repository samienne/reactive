#include "adder.h"
#include "spinner.h"
#include "testwidget.h"

#include <reactive/keyboardinput.h>

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
#include <reactive/signal/constant.h>
#include <reactive/signal/input.h>
#include <reactive/signal.h>

#include <ase/vector.h>

#include <btl/future.h>

#include <iostream>
#include <chrono>

using namespace reactive;

int main()
{
    auto textState = signal::input(widget::TextEditState{"Test123"});

    auto widgets = hbox({
        widget::label(signal::constant<std::string>("TestTest"))
            | widget::frame()
        , vbox({
                uniformGrid(3, 3)
                    .cell(0, 0, 1, 1, makeSpinner())
                    .cell(1, 1, 1, 1, makeSpinner())
                    .cell(2, 2, 1, 1, makeSpinner())
                , makeSpinner()
                    | widget::frame()
                    | onPointerMove([](reactive::PointerMoveEvent const& e)
                            {
                                std::cout << "MoveEvent: " << e.rel << " " << e.pos
                                    << std::endl;
                            })
                    | onPointerDown([](reactive::PointerButtonEvent const&)
                            {
                                std::cout << "down" << std::endl;
                            })
                , widget::label(signal::constant<std::string>("AbcTest"))
                    | widget::frame()
                , widget::textEdit(textState.handle,
                        signal::cast<widget::TextEditState>(textState.signal))
                })
        , adder()
            | widget::frame()
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

