#include "adder.h"

#include <reactive/widget/onclick.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/label.h>

#include <reactive/datasourcefromcollection.h>
#include <reactive/datasource.h>
#include <reactive/filler.h>
#include <reactive/vbox.h>
#include <reactive/hbox.h>

#include <reactive/signal/databind.h>
#include <reactive/signal/constant.h>

#include <string>

using namespace reactive;

namespace
{
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

    WidgetFactory itemEntry(
            signal::InputHandle<std::string> outHandle,
            std::function<void(std::string text)> onEnter,
            std::function<void()> onSort
            )
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
                button("Add", std::move(onEnterSignal)),
                button("Sort", signal::constant(std::move(onSort)))
            });
    }
} // anonymous namespace

reactive::WidgetFactory adder()
{
    Collection<std::string> items;

    {
        auto range = items.rangeLock();

        range.pushBack("test 1");
        range.pushBack("test 2");
        range.pushBack("test 3");
    }

    auto textInput = signal::input<std::string>("");

    auto swapState = std::make_shared<size_t>();

    auto widgets = signal::dataBind<std::string>(
            dataSourceFromCollection(items),
            [items, textInputSignal=std::move(textInput.signal), swapState]
            (Signal<std::string> value, size_t id) mutable -> WidgetFactory
            {
                return hbox({
                    button("U", signal::mapFunction([items, id]
                        (std::string str) mutable
                        {
                            auto range = items.rangeLock();
                            auto i = range.findId(id);
                            if (i != range.end())
                            {
                                range.update(i, std::move(str));
                            }
                        },
                        textInputSignal.clone()
                        ))
                    ,
                    button("T", signal::mapFunction([items, id]() mutable
                        {
                            auto range = items.rangeLock();
                            auto i = range.findId(id);

                            range.move(i, range.begin());
                        }))
                    ,
                    button("S", signal::mapFunction([items, id, swapState]() mutable
                        {
                            if (*swapState == 0)
                            {
                                *swapState = id;
                            }
                            else
                            {
                                auto range = items.rangeLock();
                                auto i = range.findId(id);
                                auto j = range.findId(*swapState);

                                range.swap(i, j);
                                *swapState = 0;
                            }
                        }))
                    ,
                    widget::label(std::move(value))
                    ,
                    hfiller()
                    ,
                    button("x", signal::constant([id, items]() mutable
                        {
                            items.rangeLock().eraseWithId(id);
                        }))
                    });
            });

    return vbox({
            vbox(std::move(widgets)),
            itemEntry(textInput.handle, [items](std::string text) mutable
                {
                    items.rangeLock().pushFront(std::move(text));
                },
                [items]() mutable
                {
                    items.rangeLock().sort();
                }
                )
            });
}

