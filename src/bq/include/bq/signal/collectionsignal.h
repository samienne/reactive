#pragma once

#include "arraysignal.h"
#include "collection.h"
#include "datacontext.h"
#include "frameinfo.h"
#include "signal.h"
#include "signalresult.h"
#include "updateresult.h"

#include <btl/connection.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bq::signal
{
    namespace detail
    {
        /**
         * @brief The snapshot of one collection that one DataContext is on.
         *
         * Keyed in the DataContext by the collection's id, so every signal
         * over the collection within one context reads one snapshot per
         * frame. The signals hold it; the DataContext only finds it.
         */
        template <typename T>
        struct CollectionFrame
        {
            using SnapshotPtr = typename Collection<T>::SnapshotPtr;

            std::size_t const* findIndex(std::size_t id)
            {
                if (!index)
                {
                    index.emplace();
                    index->reserve(snapshot->items.size());
                    for (std::size_t i = 0; i != snapshot->items.size(); ++i)
                        index->emplace(snapshot->items[i].id, i);
                }

                auto i = index->find(id);
                return i == index->end() ? nullptr : &i->second;
            }

            CollectionItem<T> const* find(std::size_t id)
            {
                std::size_t const* i = findIndex(id);
                return i ? &snapshot->items[*i] : nullptr;
            }

            uint64_t frameId = 0;
            SnapshotPtr snapshot;
            std::optional<std::unordered_map<std::size_t, std::size_t>> index;
            btl::connection connection;
        };

        /**
         * @brief Finds this context's frame of 'collection', moving it to
         * the latest snapshot the first time it is reached in a new frame.
         */
        template <typename T>
        std::shared_ptr<CollectionFrame<T>> acquireCollectionFrame(
                DataContext& context, Collection<T> const& collection,
                FrameInfo const& frame)
        {
            auto entry = context.findData<CollectionFrame<T>>(
                    collection.getId());

            if (!entry)
            {
                entry = context.initializeData<CollectionFrame<T>>(
                        collection.getId());

                // Registered before loading, so a publish in between still
                // wakes the context.
                std::weak_ptr<ObserveControl> observe =
                    context.observeControl();
                entry->connection = collection.onChange(
                        [observe=std::move(observe)](uint64_t)
                        {
                            if (auto control = observe.lock())
                                control->fire();
                        });

                entry->frameId = frame.getFrameId();
                entry->snapshot = collection.snapshot();
            }
            else if (frame.getFrameId() > entry->frameId)
            {
                entry->frameId = frame.getFrameId();
                auto latest = collection.snapshot();
                if (latest != entry->snapshot)
                {
                    entry->snapshot = std::move(latest);
                    entry->index.reset();
                }
            }

            return entry;
        }

        /**
         * @brief A signal of a collection's snapshots.
         */
        template <typename T>
        class CollectionSnapshotSignal
        {
        public:
            using SnapshotPtr = typename Collection<T>::SnapshotPtr;

            struct DataType
            {
                std::shared_ptr<CollectionFrame<T>> frame;
                SnapshotPtr snapshot;
            };

            explicit CollectionSnapshotSignal(Collection<T> collection) :
                collection_(std::move(collection))
            {
            }

            DataType initialize(DataContext& context,
                    FrameInfo const& frame) const
            {
                auto entry = acquireCollectionFrame(context, collection_,
                        frame);
                auto snapshot = entry->snapshot;
                return { std::move(entry), std::move(snapshot) };
            }

            SignalResult<SnapshotPtr const&> evaluate(DataContext&,
                    DataType const& data) const
            {
                return SignalResult<SnapshotPtr const&>(data.snapshot);
            }

            UpdateResult update(DataContext& context, DataType& data,
                    FrameInfo const& frame)
            {
                acquireCollectionFrame(context, collection_, frame);

                bool const changed = data.frame->snapshot->generation
                    != data.snapshot->generation;
                if (changed)
                    data.snapshot = data.frame->snapshot;

                return { changed };
            }

        private:
            Collection<T> collection_;
        };

        /**
         * @brief A signal of the value of one collection item.
         *
         * Changes only when that item is updated. When the item is not in
         * the collection the signal holds the last value it saw, starting
         * from 'initial', and does not change.
         */
        template <typename T>
        class CollectionItemSignal
        {
        public:
            struct DataType
            {
                std::shared_ptr<CollectionFrame<T>> frame;
                uint64_t snapshotGeneration;
                uint64_t itemGeneration;
                std::shared_ptr<T const> value;
            };

            CollectionItemSignal(Collection<T> collection,
                    CollectionItem<T> initial) :
                collection_(std::move(collection)),
                initial_(std::move(initial))
            {
            }

            DataType initialize(DataContext& context,
                    FrameInfo const& frame) const
            {
                auto entry = acquireCollectionFrame(context, collection_,
                        frame);

                auto item = entry->find(initial_.id);
                if (!item)
                    item = &initial_;

                auto const generation = entry->snapshot->generation;
                return { entry, generation, item->generation, item->value };
            }

            SignalResult<T const&> evaluate(DataContext&,
                    DataType const& data) const
            {
                return SignalResult<T const&>(*data.value);
            }

            UpdateResult update(DataContext& context, DataType& data,
                    FrameInfo const& frame)
            {
                acquireCollectionFrame(context, collection_, frame);

                auto const& snapshot = *data.frame->snapshot;
                if (snapshot.generation == data.snapshotGeneration)
                    return { false };

                data.snapshotGeneration = snapshot.generation;

                auto item = data.frame->find(initial_.id);
                if (!item || item->generation == data.itemGeneration)
                    return { false };

                data.itemGeneration = item->generation;
                data.value = item->value;

                return { true };
            }

        private:
            Collection<T> collection_;
            CollectionItem<T> initial_;
        };

        template <typename TDelegate, typename T>
        constexpr bool isCollectionForEachCallable =
            std::is_invocable_v<TDelegate const&, AnySignal<T>, std::size_t>;
    } // namespace detail

    template <typename T>
    struct IsSignal<detail::CollectionSnapshotSignal<T>> : std::true_type {};

    template <typename T>
    struct IsSignal<detail::CollectionItemSignal<T>> : std::true_type {};

    /**
     * @brief The snapshots of a collection, as a signal.
     *
     * Changes whenever the collection publishes a new generation. Within one
     * SignalContext every signal made from one collection, including the
     * items forEach() hands out, carries the same snapshot in any one frame.
     * A publish wakes the observing context.
     *
     * The signal keeps the collection's contents alive.
     */
    template <typename T>
    auto snapshotSignal(Collection<T> const& collection)
    {
        return AnySignal<typename Collection<T>::SnapshotPtr>(wrap(
                    detail::CollectionSnapshotSignal<T>(collection)));
    }

    /**
     * @brief The generation of a collection, as a signal.
     *
     * Changes whenever the collection publishes a new generation.
     */
    template <typename T>
    AnySignal<uint64_t> generationSignal(Collection<T> const& collection)
    {
        using SnapshotPtr = typename Collection<T>::SnapshotPtr;

        return snapshotSignal(collection).map([](SnapshotPtr const& snapshot)
                {
                    return snapshot->generation;
                });
    }

    /**
     * @brief Builds one value per collection item, keyed by the item's id.
     *
     * 'delegate' is invoked as 'delegate(value, id)' once per item id per
     * context, where 'value' is a signal of the item's value. An update of
     * the item reaches the built value through 'value' without rebuilding it,
     * and changes no other item's signal. A reorder rebuilds nothing. An
     * erased item's built value is released in the update that observes the
     * erase.
     *
     * The delegate is invoked as a const function.
     */
    template <typename T, typename TDelegate, typename = std::enable_if_t<
        detail::isCollectionForEachCallable<TDelegate, T>
        >>
    auto forEach(Collection<T> const& collection, TDelegate delegate)
    {
        using SnapshotPtr = typename Collection<T>::SnapshotPtr;
        using U = std::decay_t<std::invoke_result_t<
            TDelegate const&, AnySignal<T>, std::size_t>>;

        using Item = CollectionItem<T>;

        AnySignal<std::vector<Item>> items = snapshotSignal(collection).map(
                [](SnapshotPtr const& snapshot)
                {
                    return snapshot->items;
                });

        auto build = [collection, delegate=std::move(delegate)](
                std::size_t const& id, Item const& item)
            {
                return delegate(AnySignal<T>(wrap(
                                detail::CollectionItemSignal<T>(collection,
                                    item))),
                        id);
            };

        return ArraySignal<U>::fromElements(detail::makeArrayOnce(
                    std::move(items),
                    [](Item const& item)
                    {
                        return item.id;
                    },
                    std::move(build)));
    }
} // namespace bq::signal
