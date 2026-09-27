#pragma once

#include "datacontext.h"

#include <btl/connection.h>
#include <btl/typetraits.h>
#include <btl/uniqueid.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
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
        std::uint64_t id;
        std::uint64_t generation;
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
        std::uint64_t generation;
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

        CollectionIterator() = default;

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

        T const& operator[](difference_type amount) const
        {
            return *iter_[amount].value;
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

        CollectionIterator& operator+=(difference_type amount)
        {
            iter_ += amount;
            return *this;
        }

        CollectionIterator& operator-=(difference_type amount)
        {
            iter_ -= amount;
            return *this;
        }

        CollectionIterator operator+(difference_type amount) const
        {
            return CollectionIterator(iter_ + amount);
        }

        friend CollectionIterator operator+(difference_type amount,
                CollectionIterator const& iter)
        {
            return iter + amount;
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

        bool operator<=(CollectionIterator const& rhs) const
        {
            return iter_ <= rhs.iter_;
        }

        bool operator>=(CollectionIterator const& rhs) const
        {
            return iter_ >= rhs.iter_;
        }

        /**
         * @brief The id of the item this iterator points at.
         */
        std::uint64_t getId() const
        {
            return iter_->id;
        }

        /**
         * @brief The generation in which the item was inserted or last
         * updated.
         */
        std::uint64_t getGeneration() const
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
        TIter iter_{};
    };

    /**
     * @brief An ordered list of immutable items that several places can hold
     * and mutate, published as a sequence of immutable snapshots.
     *
     * Copies of a collection share one set of contents.
     *
     * Every change happens inside a write transaction, opened by write() and
     * ended when the returned Transaction is destroyed. However many
     * mutations a transaction makes, it publishes exactly one new snapshot
     * with exactly one new generation when it ends. A transaction publishes
     * nothing if every mutation it made was a no-op, or if it ends by an
     * exception propagating out of its scope, which discards all of its
     * mutations. Transactions are serialized by a non-recursive lock: opening
     * a second one on a thread that holds one deadlocks.
     *
     * Reading never waits for a writer. read() and snapshot() return the
     * latest published snapshot; while a transaction is open they keep
     * returning the one before it.
     *
     * Every item has an id, never reused by this collection, which an update
     * keeps. Ids are unique within one collection only.
     */
    template <typename T>
    class Collection
    {
    public:
        using Item = CollectionItem<T>;
        using Snapshot = CollectionSnapshot<T>;
        using SnapshotPtr = std::shared_ptr<Snapshot const>;
        using Items = std::vector<Item>;

        using Iterator = CollectionIterator<T, typename Items::const_iterator>;
        using ReverseIterator =
            CollectionIterator<T, typename Items::const_reverse_iterator>;

        using ChangeCallback = std::function<void(std::uint64_t generation)>;

    private:
        struct Control;

    public:
        /**
         * @brief Read access to one snapshot.
         *
         * Holds no lock. The snapshot it was made from stays alive and
         * unchanged for the view's lifetime.
         */
        class View
        {
        public:
            explicit View(SnapshotPtr snapshot) :
                snapshot_(std::move(snapshot))
            {
            }

            Iterator begin() const
            {
                return Iterator(snapshot_->items.cbegin());
            }

            Iterator end() const
            {
                return Iterator(snapshot_->items.cend());
            }

            ReverseIterator rbegin() const
            {
                return ReverseIterator(snapshot_->items.crbegin());
            }

            ReverseIterator rend() const
            {
                return ReverseIterator(snapshot_->items.crend());
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
            Iterator findId(std::uint64_t id) const
            {
                return Collection::findId(begin(), end(), id);
            }

            /**
             * @brief The snapshot this view reads.
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
         * the transaction see its own changes. Destroying it publishes them,
         * if there were any, as one snapshot with one new generation,
         * releases the lock, and only then invokes the change callbacks. If
         * it is destroyed by an exception propagating out of its scope it
         * discards its changes instead. A moved-from transaction does
         * nothing.
         *
         * Iterators obtained from a transaction are invalidated by any
         * mutation through it, as a 'std::vector''s are.
         */
        class Transaction
        {
        public:
            Transaction(Transaction const&) = delete;
            Transaction(Transaction&&) noexcept = default;
            Transaction& operator=(Transaction const&) = delete;
            Transaction& operator=(Transaction&&) = delete;

            ~Transaction()
            {
                release();
            }

            Iterator begin() const
            {
                return Iterator(items().cbegin());
            }

            Iterator end() const
            {
                return Iterator(items().cend());
            }

            ReverseIterator rbegin() const
            {
                return ReverseIterator(items().crbegin());
            }

            ReverseIterator rend() const
            {
                return ReverseIterator(items().crend());
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
            Iterator findId(std::uint64_t id) const
            {
                return Collection::findId(begin(), end(), id);
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
                auto const index = indexOf(position);
                Item item{
                    control_->nextItemId++,
                    generation_,
                    makeValue(std::move(value))
                };

                auto& items = working();
                auto i = items.insert(items.begin() + index, std::move(item));
                dirty_ = true;
                return Iterator(i);
            }

            /**
             * @brief Replaces the value of the item at 'position', keeping
             * its id.
             *
             * If T is equality comparable, replacing a value with an equal
             * one is not a change.
             */
            void update(Iterator position, T value)
            {
                assert(position != end());
                auto const index = indexOf(position);

                if constexpr (btl::IsEqualityComparable<T>::value)
                {
                    if (*items()[index].value == value)
                        return;
                }

                auto newValue = makeValue(std::move(value));
                auto& item = working()[index];
                item.value = std::move(newValue);
                item.generation = generation_;
                dirty_ = true;
            }

            void erase(Iterator position)
            {
                assert(position != end());
                auto const index = indexOf(position);
                auto& items = working();
                items.erase(items.begin() + index);
                dirty_ = true;
            }

            /**
             * @brief Erases the item with 'id', if there is one.
             */
            void eraseWithId(std::uint64_t id)
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
                if (a == b)
                    return;

                auto const indexA = indexOf(a);
                auto const indexB = indexOf(b);
                auto& items = working();
                std::swap(items[indexA], items[indexB]);
                dirty_ = true;
            }

            /**
             * @brief Moves the item at 'from' so that it ends up at the
             * position 'to' names; 'end()' moves it last.
             */
            void move(Iterator from, Iterator to)
            {
                assert(from != end());

                auto const f = indexOf(from);
                auto const t = to == end() ? size() - 1 : indexOf(to);
                if (f == t)
                    return;

                auto& items = working();
                auto const first = items.begin();
                if (t < f)
                    std::rotate(first + t, first + f, first + f + 1);
                else
                    std::rotate(first + f, first + f + 1, first + t + 1);

                dirty_ = true;
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

                auto& items = working();
                std::stable_sort(items.begin(), items.end(), byValue);
                dirty_ = true;
            }

        private:
            friend class Collection;

            explicit Transaction(std::shared_ptr<Control> control) :
                control_(std::move(control)),
                lock_(control_->writeMutex),
                generation_(control_->load()->generation + 1),
                uncaughtExceptions_(std::uncaught_exceptions())
            {
            }

            Items const& items() const
            {
                if (working_)
                    return working_->items;

                return control_->load()->items;
            }

            Items& working()
            {
                if (!working_)
                {
                    working_ = std::make_shared<Snapshot>(Snapshot{
                            generation_, control_->load()->items });
                }

                return working_->items;
            }

            std::size_t indexOf(Iterator const& position) const
            {
                return static_cast<std::size_t>(
                        position.base() - items().cbegin());
            }

            void release() noexcept
            {
                if (!lock_.owns_lock())
                    return;

                bool const publish = dirty_
                    && std::uncaught_exceptions() <= uncaughtExceptions_;
                if (publish)
                    control_->store(std::move(working_));

                lock_.unlock();

                if (publish)
                    control_->notify(generation_);
            }

            std::shared_ptr<Control> control_;
            std::unique_lock<std::mutex> lock_;
            std::uint64_t generation_;
            int uncaughtExceptions_;
            std::shared_ptr<Snapshot> working_;
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
         * @brief Opens a write transaction.
         */
        Transaction write()
        {
            return Transaction(control_);
        }

        /**
         * @brief Reads the latest published snapshot, without waiting for a
         * writer.
         */
        View read() const
        {
            return View(snapshot());
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
        std::uint64_t generation() const
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
         * the argument being the newest. An exception thrown by the callback
         * is caught and ignored, and the other callbacks still run.
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

        static Iterator findId(Iterator begin, Iterator end, std::uint64_t id)
        {
            for (auto i = begin; i != end; ++i)
                if (i.getId() == id)
                    return i;

            return end;
        }

        struct Callback
        {
            explicit Callback(ChangeCallback f) :
                function(std::move(f))
            {
            }

            ChangeCallback const function;
            std::atomic<bool> connected{ true };
        };

        using Callbacks = std::vector<std::shared_ptr<Callback>>;

        struct Control : std::enable_shared_from_this<Control>
        {
            SnapshotPtr load() const
            {
                return std::atomic_load(&current);
            }

            void store(SnapshotPtr snapshot) noexcept
            {
                std::atomic_store(&current, std::move(snapshot));
            }

            btl::connection addCallback(ChangeCallback function)
            {
                auto callback = std::make_shared<Callback>(std::move(function));
                {
                    std::lock_guard<std::mutex> lock(callbackMutex);
                    auto next = connectedCallbacks();
                    next->push_back(callback);
                    std::atomic_store(&callbacks,
                            std::shared_ptr<Callbacks const>(std::move(next)));
                }

                std::weak_ptr<Control> weakControl = this->shared_from_this();
                std::weak_ptr<Callback> weakCallback = callback;
                return btl::connection::on_disconnect(
                        [weakControl=std::move(weakControl),
                        weakCallback=std::move(weakCallback)]()
                        {
                            if (auto callback = weakCallback.lock())
                                callback->connected = false;

                            if (auto control = weakControl.lock())
                                control->compactCallbacks();
                        });
            }

            // Disconnecting has already happened through the flag; this only
            // frees memory, so failing to allocate is not an error.
            void compactCallbacks() noexcept
            {
                try
                {
                    std::lock_guard<std::mutex> lock(callbackMutex);
                    std::atomic_store(&callbacks,
                            std::shared_ptr<Callbacks const>(
                                connectedCallbacks()));
                }
                catch (...)
                {
                }
            }

            void notify(std::uint64_t generation) noexcept
            {
                auto const toCall = std::atomic_load(&callbacks);
                for (auto const& callback : *toCall)
                {
                    if (!callback->connected)
                        continue;

                    try
                    {
                        callback->function(generation);
                    }
                    catch (...)
                    {
                    }
                }
            }

            btl::UniqueId const id = makeUniqueId();
            std::mutex writeMutex;
            std::uint64_t nextItemId = 1;
            SnapshotPtr current =
                std::make_shared<Snapshot const>(Snapshot{ 0, {} });

            std::mutex callbackMutex;
            std::shared_ptr<Callbacks const> callbacks =
                std::make_shared<Callbacks const>();

        private:
            std::shared_ptr<Callbacks> connectedCallbacks() const
            {
                auto result = std::make_shared<Callbacks>();
                for (auto const& callback : *callbacks)
                    if (callback->connected)
                        result->push_back(callback);

                return result;
            }
        };

        std::shared_ptr<Control> control_;
    };
} // namespace bq::signal
