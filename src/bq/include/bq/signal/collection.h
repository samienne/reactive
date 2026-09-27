#pragma once

#include "datacontext.h"

#include <btl/connection.h>
#include <btl/uniqueid.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace bq::signal
{
    /**
     * @brief One item of a collection snapshot.
     *
     * 'id' names the item for as long as it stays in the collection, across
     * updates and reorderings; it is never reused within one collection.
     * 'generation' is the collection generation in which the item was
     * inserted or last updated. 'value' is immutable: an update replaces it.
     */
    template <typename T>
    struct CollectionItem
    {
        std::size_t id;
        uint64_t generation;
        std::shared_ptr<T const> value;
    };

    /**
     * @brief The immutable contents of a collection as of one generation.
     *
     * Held through a 'std::shared_ptr<CollectionSnapshot<T> const>'; a holder
     * keeps it alive and unchanged regardless of later writes.
     */
    template <typename T>
    struct CollectionSnapshot
    {
        uint64_t generation;
        std::vector<CollectionItem<T>> items;
    };

    /**
     * @brief Iterates the items of a collection, yielding their values.
     */
    template <typename T, typename TIter>
    class CollectionIterator
    {
    public:
        using value_type = T;
        using difference_type =
            typename std::iterator_traits<TIter>::difference_type;
        using pointer = T const*;
        using reference = T const&;
        using iterator_category = std::random_access_iterator_tag;

        CollectionIterator(TIter iter) :
            iter_(std::move(iter))
        {
        }

        T const& operator*() const
        {
            return *iter_->value;
        }

        T const* operator->() const
        {
            return iter_->value.get();
        }

        CollectionIterator& operator++()
        {
            ++iter_;
            return *this;
        }

        CollectionIterator operator++(int)
        {
            return CollectionIterator(iter_++);
        }

        CollectionIterator& operator--()
        {
            --iter_;
            return *this;
        }

        CollectionIterator operator--(int)
        {
            return CollectionIterator(iter_--);
        }

        CollectionIterator operator+(difference_type amount) const
        {
            return CollectionIterator(iter_ + amount);
        }

        CollectionIterator operator-(difference_type amount) const
        {
            return CollectionIterator(iter_ - amount);
        }

        difference_type operator-(CollectionIterator const& rhs) const
        {
            return iter_ - rhs.iter_;
        }

        bool operator==(CollectionIterator const& rhs) const
        {
            return iter_ == rhs.iter_;
        }

        bool operator!=(CollectionIterator const& rhs) const
        {
            return iter_ != rhs.iter_;
        }

        bool operator<(CollectionIterator const& rhs) const
        {
            return iter_ < rhs.iter_;
        }

        bool operator>(CollectionIterator const& rhs) const
        {
            return iter_ > rhs.iter_;
        }

        /**
         * @brief The id of the item this iterator points at.
         */
        std::size_t getId() const
        {
            return iter_->id;
        }

        /**
         * @brief The generation in which the item was inserted or last
         * updated.
         */
        uint64_t getGeneration() const
        {
            return iter_->generation;
        }

        /**
         * @brief The underlying iterator over the collection's items.
         */
        TIter const& base() const
        {
            return iter_;
        }

    private:
        TIter iter_;
    };

    /**
     * @brief An ordered list of immutable items that several places can hold
     * and mutate, published as a sequence of immutable snapshots.
     *
     * Copies of a collection share one set of contents.
     *
     * Every change happens inside a write transaction, opened by rangeLock()
     * and ended when the returned Range is destroyed. However many mutations
     * a transaction makes, it publishes exactly one new snapshot with exactly
     * one new generation when it ends, and a transaction that mutates nothing
     * publishes nothing. Transactions are serialized by a non-recursive lock:
     * opening a second one on a thread that holds one deadlocks.
     *
     * Reading never waits for a writer. snapshot() and crangeLock() return
     * the latest published snapshot; while a transaction is open they keep
     * returning the one before it.
     *
     * Every item has an id, drawn from a counter of this collection and never
     * reused by it, which an update keeps. Ids are unique within one
     * collection only.
     */
    template <typename T>
    class Collection
    {
    public:
        using Item = CollectionItem<T>;
        using Snapshot = CollectionSnapshot<T>;
        using SnapshotPtr = std::shared_ptr<Snapshot const>;
        using Items = std::vector<Item>;

        using Iterator = CollectionIterator<T, typename Items::iterator>;
        using ReverseIterator =
            CollectionIterator<T, typename Items::reverse_iterator>;
        using ConstIterator =
            CollectionIterator<T, typename Items::const_iterator>;
        using ConstReverseIterator =
            CollectionIterator<T, typename Items::const_reverse_iterator>;

        using ChangeCallback = std::function<void(uint64_t generation)>;

    private:
        struct Control;

    public:
        /**
         * @brief Read access to one snapshot.
         *
         * Holds no lock. The snapshot it was made from stays alive and
         * unchanged for the range's lifetime.
         */
        class ConstRange
        {
        public:
            explicit ConstRange(SnapshotPtr snapshot) :
                snapshot_(std::move(snapshot))
            {
            }

            ConstIterator begin() const
            {
                return ConstIterator(snapshot_->items.cbegin());
            }

            ConstIterator end() const
            {
                return ConstIterator(snapshot_->items.cend());
            }

            ConstReverseIterator rbegin() const
            {
                return ConstReverseIterator(snapshot_->items.crbegin());
            }

            ConstReverseIterator rend() const
            {
                return ConstReverseIterator(snapshot_->items.crend());
            }

            std::size_t size() const
            {
                return snapshot_->items.size();
            }

            T const& operator[](std::size_t index) const
            {
                return *snapshot_->items[index].value;
            }

            /**
             * @brief The item with 'id', or end() if there is none.
             */
            ConstIterator findId(std::size_t id) const
            {
                for (auto i = begin(); i != end(); ++i)
                    if (i.getId() == id)
                        return i;

                return end();
            }

            /**
             * @brief The snapshot this range reads.
             */
            SnapshotPtr const& snapshot() const
            {
                return snapshot_;
            }

        private:
            SnapshotPtr snapshot_;
        };

        /**
         * @brief A write transaction.
         *
         * Holds the collection's write lock for its lifetime. Reads through
         * the range see the transaction's own changes. Destroying it
         * publishes them, if there were any, as one snapshot with one new
         * generation, releases the lock, and only then invokes the change
         * callbacks. A moved-from range does nothing.
         *
         * Iterators obtained from a range are invalidated by any mutation
         * through it, as a 'std::vector''s are.
         */
        class Range
        {
        public:
            Range(Range const&) = delete;
            Range(Range&&) noexcept = default;
            Range& operator=(Range const&) = delete;
            Range& operator=(Range&&) = delete;

            ~Range()
            {
                release();
            }

            Iterator begin()
            {
                return Iterator(working().begin());
            }

            Iterator end()
            {
                return Iterator(working().end());
            }

            ReverseIterator rbegin()
            {
                return ReverseIterator(working().rbegin());
            }

            ReverseIterator rend()
            {
                return ReverseIterator(working().rend());
            }

            std::size_t size() const
            {
                return items().size();
            }

            T const& operator[](std::size_t index) const
            {
                return *items()[index].value;
            }

            /**
             * @brief The item with 'id', or end() if there is none.
             */
            Iterator findId(std::size_t id)
            {
                for (auto i = begin(); i != end(); ++i)
                    if (i.getId() == id)
                        return i;

                return end();
            }

            void pushBack(T value)
            {
                insert(end(), std::move(value));
            }

            void pushFront(T value)
            {
                insert(begin(), std::move(value));
            }

            /**
             * @brief Inserts a new item with a new id before 'position'.
             */
            Iterator insert(Iterator position, T value)
            {
                markDirty();
                auto& items = working();
                return Iterator(items.insert(position.base(), Item{
                            control_->nextItemId++,
                            generation_,
                            makeValue(std::move(value))
                            }));
            }

            /**
             * @brief Replaces the value of the item at 'position', keeping
             * its id.
             */
            void update(Iterator position, T value)
            {
                assert(position != end());
                markDirty();
                auto i = position.base();
                i->value = makeValue(std::move(value));
                i->generation = generation_;
            }

            void erase(Iterator position)
            {
                assert(position != end());
                markDirty();
                working().erase(position.base());
            }

            /**
             * @brief Erases the item with 'id', if there is one.
             */
            void eraseWithId(std::size_t id)
            {
                auto i = findId(id);
                if (i != end())
                    erase(i);
            }

            /**
             * @brief Exchanges the positions of two items.
             */
            void swap(Iterator a, Iterator b)
            {
                assert(a != end() && b != end());
                markDirty();
                std::iter_swap(a.base(), b.base());
            }

            /**
             * @brief Moves the item at 'from' so that it ends up at the
             * position 'to' names; 'end()' moves it last.
             */
            void move(Iterator from, Iterator to)
            {
                assert(from != end());

                auto f = from.base();
                auto t = to.base();

                if (t < f)
                {
                    markDirty();
                    std::rotate(t, f, f + 1);
                }
                else if (f < t)
                {
                    markDirty();
                    if (t != working().end())
                        ++t;
                    std::rotate(f, f + 1, t);
                }
            }

            /**
             * @brief Sorts the items by value, stably. Ids travel with their
             * values; an already sorted collection is left unchanged.
             */
            template <typename TCompare = std::less<T>>
            void sort(TCompare&& compare = TCompare())
            {
                auto byValue = [&compare](Item const& a, Item const& b)
                    {
                        return compare(*a.value, *b.value);
                    };

                Items const& current = items();
                if (std::is_sorted(current.begin(), current.end(), byValue))
                    return;

                markDirty();
                auto& items = working();
                std::stable_sort(items.begin(), items.end(), byValue);
            }

        private:
            friend class Collection;

            explicit Range(std::shared_ptr<Control> control) :
                control_(std::move(control)),
                lock_(control_->writeMutex),
                generation_(control_->load()->generation + 1)
            {
            }

            Items const& items() const
            {
                if (working_)
                    return *working_;

                return control_->load()->items;
            }

            Items& working()
            {
                if (!working_)
                    working_ = control_->load()->items;

                return *working_;
            }

            void markDirty()
            {
                working();
                dirty_ = true;
            }

            void release() noexcept
            {
                if (!lock_.owns_lock())
                    return;

                bool const publish = dirty_;
                if (publish)
                {
                    control_->store(std::make_shared<Snapshot const>(Snapshot{
                                generation_, std::move(*working_) }));
                }

                lock_.unlock();

                if (publish)
                    control_->notify(generation_);
            }

            std::shared_ptr<Control> control_;
            std::unique_lock<std::mutex> lock_;
            uint64_t generation_;
            std::optional<Items> working_;
            bool dirty_ = false;
        };

        /**
         * @brief Constructs an empty collection at generation zero.
         */
        Collection() :
            control_(std::make_shared<Control>())
        {
        }

        /**
         * @brief Copies share one set of contents.
         */
        Collection(Collection const&) = default;

        /** @overload */
        Collection& operator=(Collection const&) = default;

        /**
         * @brief Opens a write transaction.
         */
        Range rangeLock()
        {
            return Range(control_);
        }

        /**
         * @brief Reads the latest published snapshot, without waiting for a
         * writer.
         */
        ConstRange rangeLock() const
        {
            return crangeLock();
        }

        /** @overload */
        ConstRange crangeLock() const
        {
            return ConstRange(snapshot());
        }

        /**
         * @brief The latest published snapshot.
         *
         * Never waits for a writer: while a transaction is open this returns
         * the snapshot published before it.
         */
        SnapshotPtr snapshot() const
        {
            return control_->load();
        }

        /**
         * @brief The generation of the latest published snapshot.
         */
        uint64_t generation() const
        {
            return snapshot()->generation;
        }

        /**
         * @brief Identifies this collection and its copies, uniquely within
         * the process.
         */
        btl::UniqueId getId() const
        {
            return control_->id;
        }

        /**
         * @brief Registers 'callback' to be invoked after every published
         * change, with the new generation.
         *
         * The callback runs on the writing thread after the write lock is
         * released, so it may read or write the collection. Writers on
         * different threads may invoke it concurrently and out of generation
         * order; read snapshot() for the latest state rather than relying on
         * the argument being the newest. It must not throw.
         *
         * Disconnecting stops later notifications. One already in progress on
         * another thread may still invoke the callback once.
         */
        btl::connection onChange(ChangeCallback callback) const
        {
            return control_->addCallback(std::move(callback));
        }

    private:
        static std::shared_ptr<T const> makeValue(T value)
        {
            return std::make_shared<T>(std::move(value));
        }

        struct Control : std::enable_shared_from_this<Control>
        {
            SnapshotPtr load() const
            {
                return std::atomic_load(&current);
            }

            void store(SnapshotPtr snapshot)
            {
                std::atomic_store(&current, std::move(snapshot));
            }

            btl::connection addCallback(ChangeCallback callback)
            {
                std::size_t callbackId = 0;
                {
                    std::lock_guard<std::mutex> lock(callbackMutex);
                    callbackId = nextCallbackId++;
                    callbacks.emplace_back(callbackId,
                            std::make_shared<ChangeCallback const>(
                                std::move(callback)));
                }

                std::weak_ptr<Control> weak = this->shared_from_this();
                return btl::connection::on_disconnect(
                        [weak=std::move(weak), callbackId]()
                        {
                            if (auto control = weak.lock())
                                control->removeCallback(callbackId);
                        });
            }

            void removeCallback(std::size_t callbackId)
            {
                std::lock_guard<std::mutex> lock(callbackMutex);
                callbacks.erase(std::remove_if(callbacks.begin(),
                            callbacks.end(),
                            [callbackId](auto const& entry)
                            {
                                return entry.first == callbackId;
                            }),
                        callbacks.end());
            }

            void notify(uint64_t generation)
            {
                std::vector<std::shared_ptr<ChangeCallback const>> toCall;
                {
                    std::lock_guard<std::mutex> lock(callbackMutex);
                    toCall.reserve(callbacks.size());
                    for (auto const& entry : callbacks)
                        toCall.push_back(entry.second);
                }

                for (auto const& callback : toCall)
                    (*callback)(generation);
            }

            btl::UniqueId const id = makeUniqueId();
            std::mutex writeMutex;
            std::size_t nextItemId = 1;
            SnapshotPtr current =
                std::make_shared<Snapshot const>(Snapshot{ 0, {} });

            std::mutex callbackMutex;
            std::size_t nextCallbackId = 1;
            std::vector<std::pair<std::size_t,
                std::shared_ptr<ChangeCallback const>>> callbacks;
        };

        std::shared_ptr<Control> control_;
    };
} // namespace bq::signal
