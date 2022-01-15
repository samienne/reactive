#include "adder.h"
#include "avg/rendertree.h"

#include <reactive/widget/clip.h>
#include <reactive/widget/transition.h>
#include <reactive/widget/onclick.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/label.h>
#include <reactive/widget/button.h>

#include <reactive/datasourcefromcollection.h>
#include <reactive/datasource.h>
#include <reactive/filler.h>
#include <reactive/vbox.h>
#include <reactive/hbox.h>
#include <reactive/app.h>

#include <reactive/signal/databind.h>
#include <reactive/signal/constant.h>

#include <string>

using namespace reactive;

namespace
{
    widget::AnyWidget itemEntry(
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
                widget::button("Add", onEnterSignal),
                widget::button("Sort", signal::constant(std::move(onSort)))
            });
    }
} // anonymous namespace

reactive::widget::AnyWidget adder()
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
            (AnySignal<std::string> value, size_t id) mutable -> widget::AnyWidget
            {
                return hbox({
                widget::button("U", signal::mapFunction([items, id]
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
                widget::button("T", signal::mapFunction([items, id]() mutable
                    {
                        auto range = items.rangeLock();
                        auto i = range.findId(id);

                        range.move(i, range.begin());
                    }))
                ,
                widget::button("S", signal::mapFunction(
                    [items, id, swapState]() mutable
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
                widget::button("x", signal::constant([id, items]() mutable
                    {
                        app().withAnimation(
                                std::chrono::milliseconds(300),
                                avg::linearCurve,
                                [id, items]() mutable
                                {
                                    items.rangeLock().eraseWithId(id);
                                });
                    }))
                })
                | reactive::widget::transition(reactive::widget::transitionLeft())
                | reactive::widget::clip()
            ;
            });

    return vbox({
            vbox(std::move(widgets)),
            itemEntry(textInput.handle, [items](std::string text) mutable
                {
                    app().withAnimation(
                            std::chrono::milliseconds(300),
                            avg::linearCurve,
                            [&]()
                            {
                                items.rangeLock().pushFront(std::move(text));
                            });
                },
                [items]() mutable
                {
                    app().withAnimation(
                            std::chrono::milliseconds(300),
                            avg::linearCurve,
                            [&]()
                            {
                                items.rangeLock().sort();
                            });
                }
                )
            });
}

