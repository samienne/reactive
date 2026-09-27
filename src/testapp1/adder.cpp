#include "adder.h"


#include <bqui/modifier/clip.h>
#include <bqui/modifier/transition.h>
#include <bqui/modifier/onclick.h>
#include <bqui/modifier/frame.h>
#include <bqui/modifier/settheme.h>
#include <bqui/modifier/setgravity.h>
#include <bqui/modifier/setminimumsize.h>

#include <bqui/widget/textedit.h>
#include <bqui/widget/label.h>
#include <bqui/widget/button.h>
#include <bqui/widget/filler.h>
#include <bqui/widget/vbox.h>
#include <bqui/widget/hbox.h>

#include <bqui/theme.h>
#include <bqui/withanimation.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/collection.h>
#include <bq/signal/collectionsignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/signal.h>

#include <avg/curve/curves.h>
#include <avg/rendertree.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace bqui;

namespace
{
    using KeyedWidgets = std::vector<std::pair<size_t, widget::AnyWidget>>;

    template <typename T, typename TDelegate>
    bq::signal::AnySignal<KeyedWidgets> keyedWidgets(
            bq::signal::Collection<T> const& items, TDelegate delegate)
    {
        using Keyed = KeyedWidgets::value_type;

        return bq::signal::join(bq::signal::forEach(items,
                    [delegate=std::move(delegate)](
                        bq::signal::AnySignal<T> value, std::uint64_t id)
                    {
                        return bq::signal::AnySignal<Keyed>(
                                bq::signal::constant(Keyed(id,
                                        delegate(std::move(value), id))));
                    }));
    }

    widget::AnyWidget itemEntry(
            bq::signal::InputHandle<std::string> outHandle,
            std::function<void(std::string text)> onEnter,
            std::function<void()> onSort
            )
    {
        auto textState = bq::signal::makeInput(widget::TextEditState(""));
        auto handle = textState.handle;

        auto onEnterSignal = textState.signal
            .bindFirst(
                [onEnter, handle]
                (auto const state) mutable
                {
                    handle.set(widget::TextEditState(""));
                    onEnter(state.text);
                });

        auto state = textState.signal.tee(
                outHandle,
                &widget::TextEditState::text);

        return widget::hbox({
                widget::textEdit(handle, std::move(state))
                    .onEnter(onEnterSignal),
                widget::button("Add", onEnterSignal),
                widget::button("Sort", bq::signal::constant(std::move(onSort)))
            });
    }
} // anonymous namespace

bqui::widget::AnyWidget adder()
{
    bq::signal::Collection<std::string> items;

    {
        auto transaction = items.write();

        transaction.pushBack("test 1");
        transaction.pushBack("test 2");
        transaction.pushBack("test 3");
        transaction.pushBack("test 4");
    }

    auto textInput = bq::signal::makeInput<std::string>("");

    auto swapState = std::make_shared<std::uint64_t>();

    auto widgets = keyedWidgets(
            items,
            [items, textInputSignal=std::move(textInput.signal), swapState]
            (bq::signal::AnySignal<std::string> value, std::uint64_t id)
                -> widget::AnyWidget
            {
                return widget::hbox({
                widget::button("U",
                    textInputSignal.bindFirst(
                    [items, id] (std::string str) mutable
                    {
                        auto transaction = items.write();
                        auto i = transaction.findId(id);
                        if (i != transaction.end())
                        {
                            transaction.update(i, std::move(str));
                        }
                    })),
                widget::button("T", bq::signal::constant([items, id]() mutable
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        auto transaction = items.write();
                        auto i = transaction.findId(id);
                        if (i != transaction.end())
                        {
                            transaction.move(i, transaction.begin());
                        }
                    }))
                ,
                widget::button("S", bq::signal::constant(
                    [items, id, swapState]() mutable
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        if (*swapState == 0)
                        {
                            *swapState = id;
                        }
                        else
                        {
                            auto transaction = items.write();
                            auto i = transaction.findId(id);
                            auto j = transaction.findId(*swapState);
                            *swapState = 0;

                            if (i != transaction.end()
                                    && j != transaction.end())
                            {
                                transaction.swap(i, j);
                            }
                        }
                    }))
                ,
                widget::label(std::move(value))
                ,
                widget::hfiller()
                ,
                widget::button("x", bq::signal::constant([id, items]() mutable
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        items.write().eraseWithId(id);
                    }))
                })
                | modifier::transition(modifier::transitionLeft())
                | modifier::clip()
            ;
            });

    auto fancy = bq::signal::makeInput(false);

    auto theme = fancy.signal.map([](bool fancy)
            {
                if (fancy)
                {
                    Theme fancyTheme;
                    fancyTheme.setSecondary(avg::Color(0.3f, 0.0f, 0.2f));
                    return fancyTheme;
                }

                return Theme();

            });

    auto buttonTitle = fancy.signal.map([](bool fancy) -> std::string
            {
                if (fancy)
                    return "Fancy";

                return "Normal";
            });

    return widget::vbox({
            widget::vbox(std::move(widgets)),
            itemEntry(textInput.handle, [items](std::string text) mutable
                {
                    auto a = withAnimation(0.3f, avg::curve::easeInCubic);
                    items.write().pushFront(std::move(text));
                },
                [items]() mutable
                {
                    auto a = withAnimation(0.5f, avg::curve::easeInOutCubic);
                    items.write().sort();
                }
                ),
                widget::hbox({
                    widget::label("Theme:"),
                    widget::button(std::move(buttonTitle), fancy.signal.bindFirst(
                        [handle=fancy.handle](bool fancy) mutable
                        {
                            auto a = withAnimation(0.3f, avg::curve::linear);
                            handle.set(!fancy);
                        }))
                        | modifier::setMinimumWidth(250.0f)
                    })
            }
        )
        | modifier::setGravity(avg::Vector2f{ 0.5f, 1.0f })
        | modifier::setTheme(std::move(theme))
        ;
}

