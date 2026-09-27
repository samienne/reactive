#pragma once

#include "collection.h"
#include "connection.h"

#include "bqui/widget/widget.h"

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
        /**
         * @brief Blocks 'T' from being deduced from a parameter.
         *
         * A delegate passed as a lambda cannot deduce 'T' against a
         * std::function, so 'T' is fixed by the Collection argument and the
         * delegate parameter is a non-deduced context.
         */
        template <typename T>
        struct TypeIdentity
        {
            using type = T;
        };

        template <typename T>
        using TypeIdentityT = typename TypeIdentity<T>::type;

        template <typename T>
        class CollectionSignalSource
        {
        public:
            using Items = std::vector<std::pair<size_t, T>>;

            static std::shared_ptr<CollectionSignalSource> get(
                    Collection<T>& collection)
            {
                using LockType = typename Collection<T>::LockType;

                auto& control = *collection.control_;

                {
                    LockType lock(control.mutex);
                    if (auto source = control.signalSource.lock())
                        return source;
                }

                auto input = bq::signal::makeInput(Items());

                // Collection invokes its callbacks with its lock already held
                // and after the mutation is applied, so the storage is read
                // here without locking and snapshots publish in mutation order.
                auto publish = [control=&control, handle=input.handle]() mutable
                    {
                        handle.set(snapshot(*control));
                    };

                Connection connections;
                connections += collection.onInsert(
                        [publish](size_t, int, T const&) mutable { publish(); });
                connections += collection.onUpdate(
                        [publish](size_t, int, T const&) mutable { publish(); });
                connections += collection.onErase(
                        [publish](size_t) mutable { publish(); });
                connections += collection.onSwap(
                        [publish](size_t, int, size_t, int) mutable
                            { publish(); });
                connections += collection.onMove(
                        [publish](size_t, int) mutable { publish(); });
                connections += collection.onRefresh(
                        [publish](Items const&) mutable { publish(); });

                std::shared_ptr<CollectionSignalSource> source;
                {
                    LockType lock(control.mutex);
                    source = control.signalSource.lock();
                    if (!source)
                    {
                        input.handle.set(snapshot(control));
                        source.reset(new CollectionSignalSource(
                                    std::move(input.signal),
                                    std::move(connections)));
                        control.signalSource = source;
                    }
                }

                return source;
            }

            bq::signal::AnySignal<Items> const& signal() const
            {
                return signal_;
            }

        private:
            CollectionSignalSource(bq::signal::AnySignal<Items> signal,
                    Connection connections) :
                signal_(std::move(signal)),
                connections_(std::move(connections))
            {
            }

            static Items snapshot(
                    typename Collection<T>::ControlBlock const& control)
            {
                using ConstIterator = typename Collection<T>::ConstIterator;

                Items items;
                items.reserve(control.data.size());

                ConstIterator const end(control.data.end());
                for (ConstIterator i(control.data.begin()); i != end; ++i)
                    items.emplace_back(i.getId(), *i);

                return items;
            }

            bq::signal::AnySignal<Items> signal_;
            Connection connections_;
        };
    }

    /**
     * @brief A reactive view of a Collection as id/value snapshots.
     *
     * The signal carries a snapshot of the collection after every change to
     * it, each item paired with its stable 'getId()'. Every signal made from
     * one collection, and every copy of one, carries the same snapshot for a
     * given change. The subscription to the collection lives as long as any
     * such signal does.
     */
    template <typename T>
    bq::signal::AnySignal<std::vector<std::pair<size_t, T>>>
    collectionSignal(Collection<T>& collection)
    {
        using Items = typename detail::CollectionSignalSource<T>::Items;

        auto source = detail::CollectionSignalSource<T>::get(collection);

        return source->signal().map(
                [source](Items const& items)
                {
                    return items;
                });
    }

    /**
     * @brief Builds one widget per collection item, keyed by its stable id.
     *
     * The delegate is invoked once per item identity and handed the item's
     * value as a signal plus its id. A value update or a reorder reaches the
     * built widget without rebuilding it; only a genuinely new id builds one
     * more, and an erased id retires its widget without disturbing the rest.
     *
     * Identity is the item's stable heap address ('getId'): an erase and
     * re-insert across frames rebuilds, but a same-frame erase and insert whose
     * new item reuses the freed address is one identity and reuses the widget.
     */
    template <typename T>
    bq::signal::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>>
    forEach(Collection<T>& collection,
            detail::TypeIdentityT<std::function<widget::AnyWidget(
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
