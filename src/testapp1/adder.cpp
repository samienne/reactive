#include "adder.h"

#include <reactive/widget/onclick.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/label.h>

#include <reactive/filler.h>
#include <reactive/vbox.h>
#include <reactive/hbox.h>

#include <reactive/signal/databind.h>
#include <reactive/signal/constant.h>

#include <string>

using namespace reactive;

namespace
{
    WidgetFactory itemEntry(
            signal::InputHandle<std::string> outHandle,
            std::function<void(std::string text)> onEnter)
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

        auto state = signal::tee(textState.signal,
                [](widget::TextEditState const& state)
                {
                    return state.text;
                },
                outHandle);

        return hbox({
                textEdit(handle, std::move(state))
                    .onEnter(onEnterSignal),
                widget::label(signal::constant<std::string>("Add"))
                    | widget::frame()
                    | widget::onClick(0, onEnterSignal)
            });
    }

    template <typename TFunc, typename U>
    WidgetFactory button(std::string label, Signal<TFunc, U> cb)
    {
        return widget::label(signal::constant(std::move(label)))
                        | widget::frame()
                        | widget::onClick(0,
                                signal::cast<std::function<void()>>(
                                    std::move(cb)
                                    ))
                        ;
    }
} // anonymous namespace

reactive::WidgetFactory adder()
{
    Collection<std::string> items;

    items.pushBack("test 1");
    items.pushBack("test 2");
    items.pushBack("test 3");

    auto textInput = signal::input<std::string>("");

    auto widgets = signal::dataBind<std::string>(
            items,
            [items, textInputSignal=std::move(textInput.signal)]
            (Signal<std::string> value, size_t id) mutable -> WidgetFactory
            {
                return hbox({
                    button("->", signal::mapFunction([items, id]
                        (std::string str) mutable
                        {
                            auto i = items.findId(id);
                            if (i != items.end())
                            {
                                items.update(i, std::move(str));
                            }
                        },
                        textInputSignal.clone()
                        ))
                    ,
                    widget::label(std::move(value))
                    ,
                    hfiller()
                    ,
                    button("x", signal::constant([id, items]() mutable
                        {
                            items.eraseWithId(id);
                        }))
                    });
            });

    return vbox({
            vbox(std::move(widgets)),
            itemEntry(textInput.handle, [items](std::string text) mutable
                {
                    items.pushBack(text);
                })
            });
}

