#pragma once

#include "collection.h"
#include "datasource.h"

namespace reactive
{
    template <typename T>
    DataSource<T> dataSourceFromCollection(Collection<T>& collection)
    {
        auto eventPipe = stream::pipe<typename DataSource<T>::Event>();

        DataSource<T> result{std::move(eventPipe.stream), {}, {} };

        result.connection += collection.onInsert(
                [handle=eventPipe.handle](size_t id, T value)
                {
                    handle.push(typename DataSource<T>::Insert{std::move(value), id});
                });

        result.connection += collection.onUpdate(
                [handle=eventPipe.handle](size_t id, T value)
                {
                    handle.push(typename DataSource<T>::Update{std::move(value), id});
                });

        result.connection += collection.onErase(
                [handle=eventPipe.handle](size_t id)
                {
                    handle.push(typename DataSource<T>::Erase{id});
                });

        result.initialize = [collection, handle=eventPipe.handle]()
        {
            auto range = collection.rangeLock();
            for (auto i = range.begin(); i != range.end(); ++i)
            {
                handle.push(typename DataSource<T>::Insert{*i, i.getId()});
            }
        };

        return result;
    }
} // namespace reactive
