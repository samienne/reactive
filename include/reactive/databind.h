#pragma once

#include "signal/input.h"

#include <reactive/stream/iterate.h>
#include <reactive/stream/pipe.h>

#include <reactive/widget/widget.h>
#include <reactive/widget/builder.h>

#include <reactive/datasource.h>

#include <reactive/signal/evaluateoninit.h>
#include <reactive/signal/signal.h>

#include <variant>
#include <vector>
#include <variant>

namespace reactive
{
    template <typename T>
    signal::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>> dataBind(
            DataSource<T> source,
            std::function<widget::AnyWidget(signal::AnySignal<T> value, size_t id)> delegate
            )
    {
        using namespace widget;

        struct WidgetState
        {
            widget::AnyWidget widget;
            signal::InputHandle<T> valueHandle;
            size_t id;
        };

        struct State
        {
            std::vector<WidgetState> objects;
        };

        auto initial = signal::evaluateOnInit(
                [eval=std::move(source.evaluate), delegate]()
                {
                    State state;

                    for (auto&& item : eval())
                    {
                        auto input = signal::makeInput<T>(std::move(item.second));
                        state.objects.push_back(WidgetState{
                                delegate(input.signal, item.first),
                                input.handle,
                                item.first
                                });
                    }

                    return state;
                });

        auto state = stream::iterate(
            // connection is put into the lambda just to keep it alive
            [delegate, connection=std::make_shared<Connection>(
                std::move(source.connection))]
            (State state, typename DataSource<T>::Event event)
            {
                if (std::holds_alternative<typename DataSource<T>::Insert>(event))
                {
                    auto& insert = std::get<typename DataSource<T>::Insert>(event);

                    auto valueInput = signal::makeInput<T>(std::move(insert.value));

                    state.objects.insert(
                            state.objects.begin() + insert.index,
                            WidgetState{
                                delegate(std::move(valueInput.signal), insert.id),
                                std::move(valueInput.handle),
                                insert.id
                                });
                }
                else if(std::holds_alternative<typename DataSource<T>::Update>(event))
                {
                    auto& update = std::get<typename DataSource<T>::Update>(event);

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
                else if (std::holds_alternative<typename DataSource<T>::Erase>(event))
                {
                    auto& erase = std::get<typename DataSource<T>::Erase>(event);
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
                else if(std::holds_alternative<typename DataSource<T>::Swap>(event))
                {
                    auto& swap = std::get<typename DataSource<T>::Swap>(event);
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
                else if(std::holds_alternative<typename DataSource<T>::Move>(event))
                {
                    auto& move = std::get<typename DataSource<T>::Move>(event);

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
                else if(std::holds_alternative<typename DataSource<T>::Refresh>(event))
                {
                    auto& refresh = std::get<typename DataSource<T>::Refresh>(event);

                    std::vector<WidgetState> newObjects;
                    newObjects.reserve(state.objects.size());

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

        auto widgetObjects = std::move(state).map([](State const& state)
            {
                std::vector<std::pair<size_t, AnyWidget>> widgetObjects;

                for (auto const& o : state.objects)
                {
                    widgetObjects.emplace_back(o.id, o.widget);
                }

                return widgetObjects;
            });

        return widgetObjects;
    }
} // namespace reactive

