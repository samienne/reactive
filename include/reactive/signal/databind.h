#pragma once

#include <reactive/stream/iterate.h>
#include <reactive/stream/pipe.h>

#include <reactive/signal/inputhandle.h>

#include <reactive/widget/widgetobject.h>
#include <reactive/widgetfactory.h>
#include <reactive/signal.h>
#include <reactive/datasource.h>

#include <vector>

namespace reactive::signal
{
    template <typename T>
    AnySignal<std::vector<widget::WidgetObject>> dataBind(
            DataSource<T> source,
            std::function<WidgetFactory(AnySignal<T> value, size_t id)> delegate
            )
    {
        using namespace widget;

        struct WidgetState
        {
            WidgetObject widget;
            InputHandle<T> valueHandle;
            size_t id;
        };

        struct State
        {
            std::vector<WidgetState> objects;
        };

        State initial {{}};

        auto state = stream::iterate(
            // connection is put into the lambda just to keep it alive
            [delegate, connection=std::move(source.connection)]
            (State state, typename DataSource<T>::Event event)
            {
                if (event.template is<typename DataSource<T>::Insert>())
                {
                    auto& insert = event.template get<typename DataSource<T>::Insert>();

                    auto valueInput = signal::input<T>(std::move(insert.value));

                    state.objects.insert(
                            state.objects.begin() + insert.index,
                            WidgetState{
                                WidgetObject(
                                    delegate(std::move(valueInput.signal), insert.id)
                                ),
                                std::move(valueInput.handle),
                                insert.id
                                });
                }
                else if(event.template is<typename DataSource<T>::Update>())
                {
                    auto& update = event.template get<typename DataSource<T>::Update>();

                    for (auto i = state.objects.begin();
                            i != state.objects.end(); ++i)
                    {
                        if (i->id == update.id)
                        {
                            i->valueHandle.set(std::move(update.value));
                            break;
                        }
                    }
                }
                else if (event.template is<typename DataSource<T>::Erase>())
                {
                    auto& erase = event.template get<typename DataSource<T>::Erase>();

                    for (auto i = state.objects.begin();
                            i != state.objects.end(); ++i)
                    {
                        if (i->id == erase.id)
                        {
                            state.objects.erase(i);
                            break;
                        }
                    }
                }
                else if(event.template is<typename DataSource<T>::Swap>())
                {
                    auto& swap = event.template get<typename DataSource<T>::Swap>();
                    auto i = state.objects.end();
                    auto j = state.objects.end();

                    for (auto k = state.objects.begin();
                            k != state.objects.end(); ++k)
                    {
                        if (k->id == swap.id1)
                            i = k;

                        if (k->id == swap.id2)
                            j = k;

                        if (i != state.objects.end() && j != state.objects.end())
                        {
                            std::iter_swap(i, j);
                            break;
                        }
                    }
                }
                else if(event.template is<typename DataSource<T>::Move>())
                {
                    auto& move = event.template get<typename DataSource<T>::Move>();

                    assert(move.newIndex < static_cast<int>(state.objects.size()));

                    auto j = state.objects.begin() + move.newIndex;

                    for (auto i = state.objects.begin();
                            i != state.objects.end(); ++i)
                    {
                        if (i->id == move.id)
                        {
                            auto from = i;
                            auto to = j;
                            if (to < from)
                            {
                                // from cannot be end() so from+1 is ok.
                                std::rotate(to, from, from+1);
                            }
                            else if (from < to)
                            {
                                auto e = to;
                                if (to != state.objects.end())
                                    ++e;

                                std::rotate(from, from + 1, e);
                            }

                            break;
                        }
                    }
                }
                else if(event.template is<typename DataSource<T>::Refresh>())
                {
                    auto& refresh = event.template get<
                        typename DataSource<T>::Refresh>();

                    std::vector<WidgetState> newObjects;
                    newObjects.reserve(state.objects.size());

                    int idx = 0;
                    for (auto&& item : refresh.values)
                    {
                        for (auto i = state.objects.begin();
                                i != state.objects.end(); ++i)
                        {
                            if (i->id == item.first)
                            {
                                newObjects.push_back(WidgetState{
                                        std::move(i->widget),
                                        std::move(i->valueHandle),
                                        i->id
                                        });

                                break;
                            }
                        }

                        ++idx;
                    }

                    state.objects = std::move(newObjects);
                }
                else
                {
                    assert(false);
                }

                return state;
            },
            std::move(initial),
            std::move(source.input)
            );

        source.initialize();

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

