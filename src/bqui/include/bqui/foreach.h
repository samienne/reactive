#pragma once

#include "collection.h"
#include "connection.h"

#include <bqui/widget/widget.h>
#include <bqui/widget/builder.h>

#include <bq/signal/arraysignal.h>
#include <bq/signal/constant.h>
#include <bq/signal/input.h>
#include <bq/signal/signal.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace bqui
{
    namespace detail
    {
        /** @brief Blocks 'T' from being deduced from a parameter.
         *
         * A delegate passed as a lambda cannot deduce 'T' against a
         * std::function, so 'T' is fixed by the Collection argument and the
         * delegate parameter is a non-deduced context.
         */
        template <typename T>
        struct Identity
        {
            using type = T;
        };

        template <typename T>
        using IdentityT = typename Identity<T>::type;
    }

    /** @brief A reactive view of a Collection as id/value snapshots.
     *
     * The signal carries a fresh 'rangeLock()' snapshot on every change to the
     * collection, each item paired with its stable 'getId()'. The subscription
     * to the collection lives as long as the signal does. Every mutation reads
     * the live collection, so no change is missed between the initial snapshot
     * and the first update.
     */
    template <typename T>
    bq::signal::AnySignal<std::vector<std::pair<size_t, T>>>
    collectionSignal(Collection<T>& collection)
    {
        auto tick = bq::signal::makeInput<size_t>(0);

        auto connections = std::make_shared<Connection>();
        auto counter = std::make_shared<size_t>(0);

        auto bump = [handle=tick.handle, counter]() mutable
            {
                handle.set(++*counter);
            };

        *connections += collection.onInsert(
                [bump](size_t, int, T const&) mutable { bump(); });
        *connections += collection.onUpdate(
                [bump](size_t, int, T const&) mutable { bump(); });
        *connections += collection.onErase(
                [bump](size_t) mutable { bump(); });
        *connections += collection.onSwap(
                [bump](size_t, int, size_t, int) mutable { bump(); });
        *connections += collection.onMove(
                [bump](size_t, int) mutable { bump(); });
        *connections += collection.onRefresh(
                [bump](std::vector<std::pair<size_t, T>>) mutable { bump(); });

        return tick.signal.map(
                [collection, connections](size_t)
                {
                    auto range = collection.rangeLock();

                    std::vector<std::pair<size_t, T>> result;
                    result.reserve(range.size());

                    for (auto i = range.begin(); i != range.end(); ++i)
                        result.emplace_back(i.getId(), *i);

                    return result;
                });
    }

    /** @brief Builds one widget per collection item, keyed by its stable id.
     *
     * The delegate is invoked once per item identity and handed the item's
     * value as a signal plus its id. A value update or a reorder reaches the
     * built widget without rebuilding it; only a genuinely new id builds one
     * more, and an erased id retires its widget without disturbing the rest.
     */
    template <typename T>
    bq::signal::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>>
    forEach(Collection<T>& collection,
            detail::IdentityT<std::function<widget::AnyWidget(
                bq::signal::AnySignal<T> value, size_t id)>> delegate)
    {
        using Item = std::pair<size_t, T>;
        using Built = std::pair<size_t, widget::AnyWidget>;

        auto array = bq::signal::forEach(
                collectionSignal(collection),
                [](Item const& item)
                {
                    return item.first;
                },
                [delegate=std::move(delegate)](size_t id,
                    bq::signal::AnySignal<Item> item)
                {
                    auto value = item.map([](Item const& i)
                            {
                                return i.second;
                            });

                    return bq::signal::AnySignal<Built>(bq::signal::constant(
                                Built(id, delegate(std::move(value), id))));
                });

        return bq::signal::join(std::move(array));
    }
}
