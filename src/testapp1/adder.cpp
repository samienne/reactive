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
#include <bqui/datasourcefromcollection.h>
#include <bqui/datasource.h>
#include <bqui/withanimation.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/evaluateoninit.h>
#include <bq/signal/signal.h>

#include <bq/stream/iterate.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <avg/curve/curves.h>
#include <avg/rendertree.h>

#include <string>

using namespace bqui;

namespace
{
    widget::AnyWidget itemEntry(
            bq::signal::InputHandle<std::string> outHandle,
            std::function<void(std::string text)> onEnter,
            std::function<void()> onSort
            )
    {
        auto textState = bq::signal::makeInput(widget::TextEditState(""));
        auto handle = textState.handle;

        auto onEnterSignal = textState.signal
            .bindToFunction(
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

    using Items = std::vector<std::pair<size_t, std::string>>;

    /** @brief The collection's current contents, as a signal.
     *
     * Every event re-reads the collection rather than replaying the change it
     * describes. Nothing is lost by the coarseness: forEach keys the result by
     * the collection's own id, so an item that stays put is not rebuilt
     * whatever the event was.
     */
    bq::signal::AnySignal<Items> collectionItems(
            Collection<std::string>& collection)
    {
        auto source = dataSourceFromCollection(collection);
        auto evaluate = std::move(source.evaluate);

        return bq::stream::iterate(
                [evaluate, connection=std::make_shared<Connection>(
                    std::move(source.connection))]
                (Items, DataSource<std::string>::Event const&)
                {
                    return evaluate();
                },
                bq::signal::evaluateOnInit(evaluate),
                std::move(source.input)
                );
    }
} // anonymous namespace

bqui::widget::AnyWidget adder()
{
    Collection<std::string> items;

    {
        auto range = items.rangeLock();

        range.pushBack("test 1");
        range.pushBack("test 2");
        range.pushBack("test 3");
        range.pushBack("test 4");
    }

    auto textInput = bq::signal::makeInput<std::string>("");

    auto swapState = std::make_shared<size_t>();

    auto widgets = bq::signal::forEach(collectionItems(items),
            [](std::pair<size_t, std::string> const& item)
            {
                return item.first;
            },
            [items, textInputSignal=std::move(textInput.signal), swapState]
            (size_t id,
                bq::signal::AnySignal<std::pair<size_t, std::string>> item)
            -> widget::AnyWidget
            {
                auto value = item.map(
                        [](std::pair<size_t, std::string> const& i)
                        {
                            return i.second;
                        });

                return widget::hbox({
                widget::button("U",
                    textInputSignal.bindToFunction(
                    [items, id] (std::string str) mutable
                    {
                        auto range = items.rangeLock();
                        auto i = range.findId(id);
                        if (i != range.end())
                        {
                            range.update(i, std::move(str));
                        }
                    })),
                widget::button("T", bq::signal::constant([items, id]() mutable
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        auto range = items.rangeLock();
                        auto i = range.findId(id);

                        range.move(i, range.begin());
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
                widget::hfiller()
                ,
                widget::button("x", bq::signal::constant([id, items]() mutable
                    {
                        auto a = withAnimation(0.3f, avg::curve::linear);
                        items.rangeLock().eraseWithId(id);
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
                    items.rangeLock().pushFront(std::move(text));
                },
                [items]() mutable
                {
                    auto a = withAnimation(0.5f, avg::curve::easeInOutCubic);
                    items.rangeLock().sort();
                }
                ),
                widget::hbox({
                    widget::label("Theme:"),
                    widget::button(std::move(buttonTitle), fancy.signal.bindToFunction(
                        [handle=fancy.handle](bool fancy) mutable
                        {
                            auto a = withAnimation(0.3f, avg::curve::linear);
                            handle.set(!fancy);
                        }))
                        | modifier::setMinimumWidth(250.0f)
                    })
            }
        )
        | modifier::setGravity({ 0.5f, 1.0f })
        | modifier::setTheme(std::move(theme))
        ;
}

