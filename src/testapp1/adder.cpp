#include "adder.h"
#include "avg/curve/curves.h"

#include <reactive/widget/clip.h>
#include <reactive/widget/transition.h>
#include <reactive/widget/onclick.h>
#include <reactive/widget/frame.h>
#include <reactive/widget/textedit.h>
#include <reactive/widget/label.h>
#include <reactive/widget/button.h>
#include <reactive/widget/theme.h>
#include <reactive/widget/settheme.h>

#include <reactive/datasourcefromcollection.h>
#include <reactive/datasource.h>
#include <reactive/filler.h>
#include <reactive/vbox.h>
#include <reactive/hbox.h>
#include <reactive/app.h>

#include <reactive/signal/databind.h>
#include <reactive/signal/constant.h>

#include <avg/rendertree.h>

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
        range.pushBack("test 4");
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
                        app().withAnimation(
                                std::chrono::milliseconds(300),
                                avg::curve::linear,
                                [&]()
                                {
                                    auto range = items.rangeLock();
                                    auto i = range.findId(id);

                                    range.move(i, range.begin());
                                });
                    }))
                ,
                widget::button("S", signal::mapFunction(
                    [items, id, swapState]() mutable
                    {
                        app().withAnimation(
                                std::chrono::milliseconds(300),
                                avg::curve::linear,
                                [&]()
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
                                });
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
                                avg::curve::linear,
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

    auto fancy = signal::input(false);

    auto theme = signal::map([](bool fancy)
            {
                if (fancy)
                {
                    widget::Theme fancyTheme;
                    fancyTheme.setSecondary(avg::Color(0.3f, 0.0f, 0.2f));
                    return fancyTheme;
                }

                return widget::Theme();

            },
            fancy.signal
            );

    auto buttonTitle = signal::map([](bool fancy) -> std::string
            {
                if (fancy)
                    return "Fancy";

                return "Normal";
            },
            fancy.signal
            );

    return vbox({
            vbox(std::move(widgets)),
            itemEntry(textInput.handle, [items](std::string text) mutable
                {
                    app().withAnimation(
                            std::chrono::milliseconds(500),
                            avg::curve::easeInCubic,
                            [&]()
                            {
                                items.rangeLock().pushFront(std::move(text));
                            });
                },
                [items]() mutable
                {
                    app().withAnimation(
                            std::chrono::milliseconds(500),
                            avg::curve::easeInOutCubic,
                            [&]()
                            {
                                items.rangeLock().sort();
                            });
                }
                ),
                hbox({
                    widget::label("Theme:"),
                    widget::button(std::move(buttonTitle), signal::mapFunction(
                        [handle=fancy.handle](bool fancy) mutable
                        {
                            app().withAnimation(
                                    std::chrono::milliseconds(300),
                                    avg::curve::linear,
                                    [&]()
                                    {
                                        handle.set(!fancy);
                                    });
                        },
                        fancy.signal
                        ))
                    })
            }
        )
        | widget::setTheme(std::move(theme))
        ;
}

