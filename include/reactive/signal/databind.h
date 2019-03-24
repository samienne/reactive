#pragma once

#include <reactive/stream/iterate.h>
#include <reactive/stream/pipe.h>

#include <reactive/signal/inputhandle.h>

#include <reactive/widget/widgetobject.h>
#include <reactive/widgetfactory.h>
#include <reactive/signal.h>
#include <reactive/collection.h>

#include <vector>

namespace reactive::signal
{
    template <typename T>
    Signal<std::vector<widget::WidgetObject>> dataBind(
            Collection<T> collection,
            std::function<WidgetFactory(Signal<T> value, size_t id)> delegate
            )
    {
        using namespace widget;

        struct Insert
        {
            T value;
            size_t id;
        };

        struct Update
        {
        };

        struct Erase
        {
        };

        struct WidgetState
        {
            WidgetObject widget;
            InputHandle<T> valueHandle;
        };

        struct State
        {
            std::vector<WidgetState> objects;
            Collection<T> collection;
        };

        State initial {{}, std::move(collection)};

        for (auto i = initial.collection.begin(); i != initial.collection.end();
                ++i)
        {
            auto valueInput = signal::input<T>(std::move(*i));

            initial.objects.push_back(
                    WidgetState{
                        WidgetObject(
                            delegate(std::move(valueInput.signal), i.getId())
                        ),
                        std::move(valueInput.handle)
                        });
        }

        auto eventPipe = stream::pipe<btl::variant<Insert, Update, Erase>>();

        Connection connection;
        connection += initial.collection.onInsert(
                [handle=eventPipe.handle](size_t id, T value)
                {
                    handle.push(Insert{std::move(value), id});
                });

        auto state = stream::iterate(
            // connection is put into the lambda just to keep it alive
            [delegate, connection=std::move(connection)]
            (State state, btl::variant<Insert, Update, Erase> event)
            {
                if (event.template is<Insert>())
                {
                    auto& insert = event.template get<Insert>();

                    auto valueInput = signal::input<T>(std::move(insert.value));

                    state.objects.push_back(
                            WidgetState{
                                WidgetObject(
                                    delegate(std::move(valueInput.signal), insert.id)
                                ),
                                std::move(valueInput.handle)
                                });
                }
                else if(event.template is<Update>())
                {
                }
                else if (event.template is<Erase>())
                {
                }

                return state;
            },
            std::move(initial),
            std::move(eventPipe.stream)
            );

        auto widgetObjects = signal::map(
            [](State const& state)
            {
                std::vector<WidgetObject> widgetObjects;

                for (auto const& o : state.objects)
                {
                    widgetObjects.push_back(o.widget);
                }

                return widgetObjects;
            },
            std::move(state)
            );

        return widgetObjects;
    }
} // namespace reactive::signal

