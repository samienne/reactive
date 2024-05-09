#pragma once

#include "stream/pipe.h"

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
                [handle=eventPipe.handle](size_t id, int index, T value)
                {
                    handle.push(typename DataSource<T>::Insert{
                            std::move(value),
                            index,
                            id});
                });

        result.connection += collection.onUpdate(
                [handle=eventPipe.handle](size_t id, int index, T value)
                {
                    handle.push(typename DataSource<T>::Update{
                            std::move(value),
                            index,
                            id});
                });

        result.connection += collection.onErase(
                [handle=eventPipe.handle](size_t id)
                {
                    handle.push(typename DataSource<T>::Erase{id});
                });

        result.connection += collection.onSwap(
                [handle=eventPipe.handle]
                (size_t id1, int index1, size_t id2, int index2)
                {
                    handle.push(typename DataSource<T>::Swap{
                            id1, index1, id2, index2
                            });
                });

        result.connection += collection.onMove(
                [handle=eventPipe.handle](size_t id, int index)
                {
                    handle.push(typename DataSource<T>::Move{id, index});
                });

        result.connection += collection.onRefresh(
                [handle=eventPipe.handle]
                (std::vector<std::pair<size_t, T>> values)
                {
                    handle.push(typename DataSource<T>::Refresh{
                            std::move(values)
                            });
                });

        result.evaluate = [collection]()
        {
            auto range = collection.rangeLock();
            std::vector<std::pair<size_t, T>> result;

            for (auto i = range.begin(); i != range.end(); ++i)
                result.emplace_back(i.getId(), *i);

            return result;
        };

        return result;
    }
} // namespace reactive
