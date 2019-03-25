#include "adder.h"

#include <reactive/widget/onclick.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/label.h>

#include <reactive/vbox.h>
#include <reactive/hbox.h>

#include <reactive/signal/databind.h>
#include <reactive/signal/constant.h>

#include <string>

using namespace reactive;

namespace
{
    WidgetFactory itemEntry(std::function<void(std::string text)> onEnter)
    {
        auto textState = signal::input(widget::TextEditState(""));
        auto handle = textState.handle;

        auto onEnterSignal = signal::share(signal::mapFunction(
                [onEnter, handle]
                (widget::TextEditState const& state) mutable
                {
                    handle.set(widget::TextEditState(""));
                    onEnter(state.text);
                },
                textState.signal
                ));

        return hbox({
                    textEdit(handle, textState.signal)
                        .onEnter(onEnterSignal),
                    widget::label(signal::constant<std::string>("Add"))
                        | widget::frame()
                        | widget::onClick(0, onEnterSignal)
                });
    }
} // anonymous namespace

reactive::WidgetFactory adder()
{
    Collection<std::string> items;

    items.pushBack("test 1");
    items.pushBack("test 2");
    items.pushBack("test 3");

    auto widgets = signal::dataBind<std::string>(
            items,
            [items]
            (Signal<std::string> value, size_t id) mutable -> WidgetFactory
            {
                return widget::label(std::move(value))
                    | widget::onClick(0,
                        [id, items](ClickEvent const&) mutable
                        {
                            items.eraseWithId(id);
                        });
            });

    return vbox({
            vbox(std::move(widgets)),
            itemEntry([items](std::string text) mutable
                {
                    items.pushBack(text);
                })
            });
}

