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

int main2()
{
    auto textState = signal::input(widget::TextEditState{"Jepskuukkuu"});

#if 1
    auto widgets = hbox({
        widget::label(signal::constant("Jepskukkuu"))
            | widget::frame()
        , vbox({
                uniformGrid(3, 3)
                    .cell(0, 0, 1, 1, makeSpinner())
                    .cell(1, 1, 1, 1, makeSpinner())
                    .cell(2, 2, 1, 1, makeSpinner())
                , makeSpinner()
                    | onPointerDown([](ase::PointerButtonEvent const& e) {
                        std::cout << "Down: " << e << std::endl;
                        })
                    | onPointerUp([]() {
                        std::cout << "Up" << std::endl;
                        })
                    | onClick(0, [](ClickEvent const&) {
                        std::cout << "Click" << std::endl;
                        })
                    | widget::frame()
                , widget::label(signal::constant("kokkelis"))
                    | widget::frame()
                , widget::textEdit(textState.handle, textState.signal)
                //, w1
                })
        //, makeTestWidget()
            //| widget::frame()
        , adder()
            | widget::frame()
    });
#else
    auto widgets = adder();
    //auto widgets = makeTestWidget();
#endif

    auto running = signal::input(true);

    std::cout << "KeyboardInput size: " << sizeof(KeyboardInput) << std::endl;
    std::cout << "Widget size: " << sizeof(Widget) << std::endl;
    std::cout << "signal::InputHandle<bool> size: " <<
        sizeof(btl::option<signal::InputHandle<bool>>) << std::endl;

    return App()
        .windows({
                window(
                    signal::constant("Functional test"),
                    std::move(widgets)
                    | debug::drawKeyboardInputs()
                    | widget::focusGroup()
                    )
                .onClose(send(false, running.handle))
                })
        .run(running.signal);
}

int main()
{
    return main2();
}

