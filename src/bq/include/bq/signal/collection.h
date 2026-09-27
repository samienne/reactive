#pragma once

#include "datacontext.h"

#include <btl/connection.h>
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
#include <type_traits>
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

    template <typename T>
    class Collection;

    template <typename T>
    class CollectionSnapshot;

    template <typename TIterator, typename THolder>
    class CollectionItems;

    template <typename TIterator, typename THolder>
    class CollectionValues;

    /**
     * @brief Iterates the items of a collection snapshot or transaction.
     *
     * A random access iterator yielding 'CollectionItem<T> const&'. 'TOwner'
     * is the snapshot or transaction it belongs to; the iterator names a
     * position in it.
     */
    template <typename T, typename TOwner>
    class CollectionIterator
    {
    public:
        using value_type = CollectionItem<T>;
        using difference_type = std::ptrdiff_t;
        using pointer = CollectionItem<T> const*;
        using reference = CollectionItem<T> const&;
        using iterator_category = std::random_access_iterator_tag;

        CollectionIterator() = default;

        reference operator*() const
        {
            return (*this)[0];
        }

        pointer operator->() const
        {
            return &(*this)[0];
        }

        reference operator[](difference_type amount) const
        {
            assert(owner_);
            return owner_->itemVector()[
                static_cast<std::size_t>(index_ + amount)];
        }

        CollectionIterator& operator++()
        {
            ++index_;
            return *this;
        }

        CollectionIterator operator++(int)
        {
            return CollectionIterator(owner_, index_++);
        }

        CollectionIterator& operator--()
        {
            --index_;
            return *this;
        }

        CollectionIterator operator--(int)
        {
            return CollectionIterator(owner_, index_--);
        }

        CollectionIterator& operator+=(difference_type amount)
        {
            index_ += amount;
            return *this;
        }

        CollectionIterator& operator-=(difference_type amount)
        {
            index_ -= amount;
            return *this;
        }

        CollectionIterator operator+(difference_type amount) const
        {
            return CollectionIterator(owner_, index_ + amount);
        }

        friend CollectionIterator operator+(difference_type amount,
                CollectionIterator const& iter)
        {
            return iter + amount;
        }

        CollectionIterator operator-(difference_type amount) const
        {
            return CollectionIterator(owner_, index_ - amount);
        }

        difference_type operator-(CollectionIterator const& rhs) const
        {
            assert(owner_ == rhs.owner_);
            return index_ - rhs.index_;
        }

        bool operator==(CollectionIterator const& rhs) const
        {
            assert(owner_ == rhs.owner_);
            return index_ == rhs.index_;
        }

        bool operator!=(CollectionIterator const& rhs) const
        {
            return !(*this == rhs);
        }

        bool operator<(CollectionIterator const& rhs) const
        {
            return *this - rhs < 0;
        }

        bool operator>(CollectionIterator const& rhs) const
        {
            return rhs < *this;
        }

        bool operator<=(CollectionIterator const& rhs) const
        {
            return !(rhs < *this);
        }

        bool operator>=(CollectionIterator const& rhs) const
        {
            return !(*this < rhs);
        }

    private:
        friend std::remove_const_t<TOwner>;

        template <typename TIterator, typename THolder>
        friend class CollectionItems;

        CollectionIterator(TOwner* owner, difference_type index) :
            owner_(owner),
            index_(index)
        {
        }

        TOwner* owner_ = nullptr;
        difference_type index_ = 0;
    };

    /**
     * @brief Iterates the values of a collection's items.
     *
     * A random access iterator yielding 'T const&', over the positions of the
     * item iterator 'TIterator' it adapts. base() is the item iterator at
     * the same position; the two kinds do not convert or compare implicitly.
     */
    template <typename TIterator>
    class CollectionValueIterator
    {
        using Value = std::remove_reference_t<
            decltype(*std::declval<TIterator const&>()->value)>;

    public:
        using value_type = std::remove_const_t<Value>;
        using difference_type = std::ptrdiff_t;
        using pointer = Value*;
        using reference = Value&;
        using iterator_category = std::random_access_iterator_tag;

        CollectionValueIterator() = default;

        explicit CollectionValueIterator(TIterator iter) :
            iter_(std::move(iter))
        {
        }

        reference operator*() const
        {
            return *iter_->value;
        }

        pointer operator->() const
        {
            return iter_->value.get();
        }

        reference operator[](difference_type amount) const
        {
            return *iter_[amount].value;
        }

        CollectionValueIterator& operator++()
        {
            ++iter_;
            return *this;
        }

        CollectionValueIterator operator++(int)
        {
            return CollectionValueIterator(iter_++);
        }

        CollectionValueIterator& operator--()
        {
            --iter_;
            return *this;
        }

        CollectionValueIterator operator--(int)
        {
            return CollectionValueIterator(iter_--);
        }

        CollectionValueIterator& operator+=(difference_type amount)
        {
            iter_ += amount;
            return *this;
        }

        CollectionValueIterator& operator-=(difference_type amount)
        {
            iter_ -= amount;
            return *this;
        }

        CollectionValueIterator operator+(difference_type amount) const
        {
            return CollectionValueIterator(iter_ + amount);
        }

        friend CollectionValueIterator operator+(difference_type amount,
                CollectionValueIterator const& iter)
        {
            return iter + amount;
        }

        CollectionValueIterator operator-(difference_type amount) const
        {
            return CollectionValueIterator(iter_ - amount);
        }

        difference_type operator-(CollectionValueIterator const& rhs) const
        {
            return iter_ - rhs.iter_;
        }

        bool operator==(CollectionValueIterator const& rhs) const
        {
            return iter_ == rhs.iter_;
        }

        bool operator!=(CollectionValueIterator const& rhs) const
        {
            return iter_ != rhs.iter_;
        }

        bool operator<(CollectionValueIterator const& rhs) const
        {
            return iter_ < rhs.iter_;
        }

        bool operator>(CollectionValueIterator const& rhs) const
        {
            return iter_ > rhs.iter_;
        }

        bool operator<=(CollectionValueIterator const& rhs) const
        {
            return iter_ <= rhs.iter_;
        }

        bool operator>=(CollectionValueIterator const& rhs) const
        {
            return iter_ >= rhs.iter_;
        }

        /**
         * @brief The item iterator at the same position.
         */
        TIterator const& base() const
        {
            return iter_;
        }

    private:
        TIterator iter_{};
    };

    /**
     * @brief The items of a collection snapshot or transaction, as a range.
     *
     * Yields 'CollectionItem<T> const&'. A range from a snapshot or a view
     * shares ownership of the snapshot: the range and its iterators stay
     * valid while the range or a copy of it is alive. A range from a
     * transaction is valid only as long as the transaction, and its
     * iterators follow the transaction's invalidation rules.
     */
    template <typename TIterator, typename THolder>
    class CollectionItems
    {
    public:
        using Iterator = TIterator;
        using ReverseIterator = std::reverse_iterator<Iterator>;

        Iterator begin() const
        {
            return Iterator(&*owner_, 0);
        }

        Iterator end() const
        {
            return Iterator(&*owner_, static_cast<std::ptrdiff_t>(size()));
        }

        ReverseIterator rbegin() const
        {
            return ReverseIterator(end());
        }

        ReverseIterator rend() const
        {
            return ReverseIterator(begin());
        }

        std::size_t size() const
        {
            return owner_->itemVector().size();
        }

        bool empty() const
        {
            return size() == 0;
        }

        typename Iterator::reference operator[](std::size_t index) const
        {
            return owner_->itemVector()[index];
        }

    private:
        template <typename T>
        friend class CollectionSnapshot;

        template <typename T>
        friend class Collection;

        friend class CollectionValues<TIterator, THolder>;

        explicit CollectionItems(THolder owner) :
            owner_(std::move(owner))
        {
        }

        THolder owner_;
    };

    /**
     * @brief The values of a collection snapshot or transaction, as a range.
     *
     * Yields 'T const&', and otherwise behaves as CollectionItems, including
     * its lifetime rules.
     */
    template <typename TIterator, typename THolder>
    class CollectionValues
    {
    public:
        using Iterator = CollectionValueIterator<TIterator>;
        using ReverseIterator = std::reverse_iterator<Iterator>;

        Iterator begin() const
        {
            return Iterator(items_.begin());
        }

        Iterator end() const
        {
            return Iterator(items_.end());
        }

        ReverseIterator rbegin() const
        {
            return ReverseIterator(end());
        }

        ReverseIterator rend() const
        {
            return ReverseIterator(begin());
        }

        std::size_t size() const
        {
            return items_.size();
        }

        bool empty() const
        {
            return items_.empty();
        }

        typename Iterator::reference operator[](std::size_t index) const
        {
            return *items_[index].value;
        }

    private:
        template <typename T>
        friend class CollectionSnapshot;

        template <typename T>
        friend class Collection;

        explicit CollectionValues(THolder owner) :
            items_(std::move(owner))
        {
        }

        CollectionItems<TIterator, THolder> items_;
    };

    namespace detail
    {
        template <typename TIterator>
        TIterator findCollectionId(TIterator begin, TIterator end,
                std::uint64_t id)
        {
            return std::find_if(begin, end, [id](auto const& item)
                    {
                        return item.id == id;
                    });
        }
    } // namespace detail

    /**
     * @brief The immutable contents of a collection as of one generation.
     *
     * Exists only behind the 'std::shared_ptr<CollectionSnapshot<T> const>'
     * a collection hands out; a holder keeps it alive and unchanged
     * regardless of later writes. items() and values() share that
     * ownership.
     */
    template <typename T>
    class CollectionSnapshot :
        public std::enable_shared_from_this<CollectionSnapshot<T>>
    {
        struct Key
        {
            explicit Key() = default;
        };

    public:
        using Item = CollectionItem<T>;
        using Iterator = CollectionIterator<T, CollectionSnapshot const>;
        using Items = CollectionItems<Iterator,
                std::shared_ptr<CollectionSnapshot const>>;
        using Values = CollectionValues<Iterator,
                std::shared_ptr<CollectionSnapshot const>>;
        using ValueIterator = typename Values::Iterator;

        CollectionSnapshot(Key, std::uint64_t generation,
                std::vector<Item> items) :
            generation_(generation),
            items_(std::move(items))
        {
        }

        CollectionSnapshot(CollectionSnapshot const&) = delete;
        CollectionSnapshot& operator=(CollectionSnapshot const&) = delete;

        std::uint64_t getGeneration() const
        {
            return generation_;
        }

        Items items() const
        {
            return Items(this->shared_from_this());
        }

        Values values() const
        {
            return Values(this->shared_from_this());
        }

        std::size_t size() const
        {
            return items_.size();
        }

        bool empty() const
        {
            return items_.empty();
        }

        /**
         * @brief The item with 'id', or the end of items() if there is none.
         *
         * Valid while the snapshot is alive.
         */
        Iterator findId(std::uint64_t id) const
        {
            return detail::findCollectionId(Iterator(this, 0),
                    Iterator(this, static_cast<std::ptrdiff_t>(size())), id);
        }

    private:
        template <typename U>
        friend class Collection;

        friend Iterator;
        friend Items;

        std::vector<Item> const& itemVector() const
        {
            return items_;
        }

        std::uint64_t generation_;
        std::vector<Item> items_;
    };

    /**
     * @brief An ordered list of immutable items that several places can hold
     * and mutate, published as a sequence of immutable snapshots.
     *
     * Copies of a collection share one set of contents. Every change happens
     * inside a write transaction, opened by write(); reading, through read()
     * or getSnapshot(), never waits for a writer.
     *
     * Snapshots, views and transactions are not ranges themselves: iterate
     * their items() or their values().
     */
    template <typename T>
    class Collection
    {
    public:
        using Item = CollectionItem<T>;
        using Snapshot = CollectionSnapshot<T>;
        using SnapshotPtr = std::shared_ptr<Snapshot const>;

        using ChangeCallback = std::function<void(std::uint64_t generation)>;

    private:
        using ItemVector = std::vector<Item>;

        struct Control;

    public:
        /**
         * @brief Read access to one snapshot.
         *
         * Holds no lock. The snapshot it was made from stays alive and
         * unchanged for the view's lifetime, and for that of the ranges
         * made from it, which may outlive the view.
         */
        class View
        {
        public:
            using Iterator = typename Snapshot::Iterator;
            using Items = typename Snapshot::Items;
            using Values = typename Snapshot::Values;
            using ValueIterator = typename Values::Iterator;

            explicit View(SnapshotPtr snapshot) :
                snapshot_(std::move(snapshot))
            {
            }

            Items items() const
            {
                return snapshot_->items();
            }

            Values values() const
            {
                return snapshot_->values();
            }

            std::size_t size() const
            {
                return snapshot_->size();
            }

            bool empty() const
            {
                return snapshot_->empty();
            }

            /**
             * @brief The item with 'id', or the end of items() if there is
             * none.
             *
             * Valid while the snapshot is alive.
             */
            Iterator findId(std::uint64_t id) const
            {
                return snapshot_->findId(id);
            }

            /**
             * @brief The snapshot this view reads.
             */
            SnapshotPtr const& getSnapshot() const
            {
                return snapshot_;
            }

        private:
            SnapshotPtr snapshot_;
        };

        /**
         * @brief A write transaction.
         *
         * Holds the collection's write lock for its lifetime, so it is bound
         * to the scope and thread that opened it and can be neither copied
         * nor moved. Transactions are serialized by a non-recursive lock:
         * opening a second one on a thread that holds one deadlocks.
         *
         * Reads through the transaction see its own changes. However many
         * mutations it makes, destroying it publishes them as one snapshot
         * with one new generation, releases the lock, and only then invokes
         * the change callbacks. It publishes nothing if every mutation was a
         * no-op, and if it is destroyed by an exception propagating out of
         * its scope it discards its changes instead.
         *
         * items() and values() are valid only as long as the transaction,
         * so neither is available on an rvalue. Their iterators name
         * positions, and the mutations take either kind: insert and erase
         * invalidate those at or after the position they change, and the
         * other mutations invalidate none.
         */
        class Transaction
        {
        public:
            using Iterator = CollectionIterator<T, Transaction>;
            using Items = CollectionItems<Iterator, Transaction*>;
            using Values = CollectionValues<Iterator, Transaction*>;
            using ValueIterator = typename Values::Iterator;
            using ConstIterator = CollectionIterator<T, Transaction const>;
            using ConstItems = CollectionItems<ConstIterator,
                  Transaction const*>;
            using ConstValues = CollectionValues<ConstIterator,
                  Transaction const*>;
            using ConstValueIterator = typename ConstValues::Iterator;

            Transaction(Transaction const&) = delete;
            Transaction(Transaction&&) = delete;
            Transaction& operator=(Transaction const&) = delete;
            Transaction& operator=(Transaction&&) = delete;

            ~Transaction()
            {
                release();
            }

            Items items() &
            {
                return Items(this);
            }

            ConstItems items() const&
            {
                return ConstItems(this);
            }

            Items items() && = delete;
            ConstItems items() const&& = delete;

            Values values() &
            {
                return Values(this);
            }

            ConstValues values() const&
            {
                return ConstValues(this);
            }

            Values values() && = delete;
            ConstValues values() const&& = delete;

            std::size_t size() const
            {
                return itemVector().size();
            }

            bool empty() const
            {
                return itemVector().empty();
            }

            /**
             * @brief The item with 'id', or the end of items() if there is
             * none.
             */
            Iterator findId(std::uint64_t id)
            {
                auto const all = items();
                return detail::findCollectionId(all.begin(), all.end(), id);
            }

            ConstIterator findId(std::uint64_t id) const
            {
                auto const all = items();
                return detail::findCollectionId(all.begin(), all.end(), id);
            }

            void pushBack(T value)
            {
                insert(items().end(), std::move(value));
            }

            void pushFront(T value)
            {
                insert(items().begin(), std::move(value));
            }

            /**
             * @brief Inserts a new item with a new id before 'position'.
             */
            Iterator insert(Iterator position, T value)
            {
                auto const index = indexOf(position);
                assert(index <= size());
                Item item{
                    control_->nextItemId++,
                    generation_,
                    makeValue(std::move(value))
                };

                auto& items = working();
                items.insert(items.begin() + index, std::move(item));
                dirty_ = true;
                return position;
            }

            ValueIterator insert(ValueIterator position, T value)
            {
                return ValueIterator(insert(position.base(),
                            std::move(value)));
            }

            /**
             * @brief Replaces the value of the item at 'position', keeping
             * its id.
             *
             * Always a change, even if the new value equals the old one.
             */
            void update(Iterator position, T value)
            {
                auto const index = indexOf(position);
                assert(index < size());

                auto newValue = makeValue(std::move(value));
                auto& item = working()[index];
                item.value = std::move(newValue);
                item.generation = generation_;
                dirty_ = true;
            }

            void update(ValueIterator position, T value)
            {
                update(position.base(), std::move(value));
            }

            void erase(Iterator position)
            {
                auto const index = indexOf(position);
                assert(index < size());
                auto& items = working();
                items.erase(items.begin() + index);
                dirty_ = true;
            }

            void erase(ValueIterator position)
            {
                erase(position.base());
            }

            /**
             * @brief Erases the item with 'id', if there is one.
             */
            void eraseWithId(std::uint64_t id)
            {
                auto i = findId(id);
                if (i != items().end())
                    erase(i);
            }

            /**
             * @brief Exchanges the positions of two items.
             */
            void swap(Iterator a, Iterator b)
            {
                auto const indexA = indexOf(a);
                auto const indexB = indexOf(b);
                assert(indexA < size() && indexB < size());
                if (indexA == indexB)
                    return;

                auto& items = working();
                std::swap(items[indexA], items[indexB]);
                dirty_ = true;
            }

            void swap(ValueIterator a, ValueIterator b)
            {
                swap(a.base(), b.base());
            }

            /**
             * @brief Moves the item at 'from' so that it ends up at the
             * position 'to' names; the end of items() moves it last.
             */
            void move(Iterator from, Iterator to)
            {
                auto const f = indexOf(from);
                auto const target = indexOf(to);
                assert(f < size() && target <= size());

                auto const t = target == size() ? size() - 1 : target;
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

            void move(ValueIterator from, ValueIterator to)
            {
                move(from.base(), to.base());
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

                ItemVector const& current = itemVector();
                if (std::is_sorted(current.begin(), current.end(), byValue))
                    return;

                // Sorted aside, so a throwing comparator leaves the items
                // as they were.
                ItemVector sorted = current;
                std::stable_sort(sorted.begin(), sorted.end(), byValue);

                if (working_)
                {
                    working_->items_.swap(sorted);
                }
                else
                {
                    working_ = std::make_shared<Snapshot>(
                            typename Snapshot::Key(), generation_,
                            std::move(sorted));
                }

                dirty_ = true;
            }

        private:
            friend class Collection;

            friend Iterator;
            friend ConstIterator;
            friend Items;
            friend ConstItems;

            explicit Transaction(std::shared_ptr<Control> control) :
                control_(std::move(control)),
                lock_(control_->writeMutex),
                published_(control_->load()),
                generation_(published_->getGeneration() + 1),
                uncaughtExceptions_(std::uncaught_exceptions())
            {
            }

            ItemVector const& itemVector() const
            {
                if (working_)
                    return working_->items_;

                return published_->items_;
            }

            ItemVector& working()
            {
                if (!working_)
                {
                    working_ = std::make_shared<Snapshot>(
                            typename Snapshot::Key(), generation_,
                            published_->items_);
                }

                return working_->items_;
            }

            std::size_t indexOf(Iterator const& position) const
            {
                assert(position.owner_ == this);
                return static_cast<std::size_t>(position.index_);
            }

            void release() noexcept
            {
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
            SnapshotPtr published_;
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
         *
         * The change is published when the transaction ends, so a scope
         * guard that must contain the publish has to be opened before the
         * transaction.
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
            return View(getSnapshot());
        }

        /**
         * @brief The latest published snapshot.
         *
         * Never waits for a writer: while a transaction is open this returns
         * the snapshot published before it.
         */
        SnapshotPtr getSnapshot() const
        {
            return control_->load();
        }

        /**
         * @brief The generation of the latest published snapshot.
         */
        std::uint64_t getGeneration() const
        {
            return getSnapshot()->getGeneration();
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
         * order; read getSnapshot() for the latest state rather than relying on
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
            SnapshotPtr current = std::make_shared<Snapshot>(
                    typename Snapshot::Key(), 0, ItemVector());

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
