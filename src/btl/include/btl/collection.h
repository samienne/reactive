#pragma once

#include "observable.h"

#include <vector>
#include <mutex>
#include <algorithm>

namespace btl
{
    template <typename T>
    class reader_t;

    template <typename T>
    auto reader(T const& collection)
    -> decltype(std::declval<T>().reader())
    {
        return collection.reader();
    }

    template <typename T, typename TCallback>
    auto observe(T const& collection, TCallback&& cb)
    -> decltype(collection.on_changed(std::forward<TCallback>(cb)))
    {
        return collection.on_changed(std::forward<TCallback>(cb));
    }

    template <typename T>
    struct collection_event
    {
        std::vector<std::pair<size_t, T const*>> added;
        std::vector<size_t> removed;
        std::vector<std::pair<size_t, size_t>> moved;
        std::vector<std::pair<size_t, T const*>> updated;
    };

    template <typename T>
    struct persistent_collection_event
    {
        std::vector<std::pair<size_t, T>> added;
        std::vector<size_t> removed;
        std::vector<std::pair<size_t, size_t>> moved;
        std::vector<std::pair<size_t, T>> updated;
    };

    template <typename T>
    auto persist(collection_event<T> const& event)
        -> persistent_collection_event<T>
    {
        persistent_collection_event<T> result;

        for (auto const& e : event.added)
            result.added.push_back(std::make_pair(e.first, *e.second));

        result.removed = event.removed;
        result.moved = event.moved;

        for (auto const& e : event.updated)
            result.updated.push_back(std::make_pair(e.first, *e.second));

        return std::move(result);
    }

    namespace detail
    {
        template <typename T>
        class collection_deferred :
            public std::enable_shared_from_this<collection_deferred<T>>
        {
        public:
            using mutex_t = SpinLock;
            using lock_t = std::unique_lock<mutex_t>;

            std::vector<T> values_;
            mutable observable<void(collection_event<T> const&)> on_changed_;
            mutex_t mutex_;
        };

        template <typename T>
        class collection_ref
        {
        public:
            collection_ref(collection_event<T>& event, T& value, size_t index) :
                event_(event),
                value_(value),
                index_(index)
            {
            }

            operator T&()
            {
                event_.updated.push_back(std::make_pair(index_, nullptr));
                return value_;
            }

            T& operator=(T const& rhs)
            {
                event_.updated.push_back(std::make_pair(index_, nullptr));
                value_ = rhs;
                return value_;
            }

        private:
            collection_event<T>& event_;
            T& value_;
            size_t index_;
        };

        template <typename T>
        class collection_iterator
        {
        public:
            collection_iterator(
                    typename std::vector<T>::const_iterator iter,
                    size_t index) :
                iter_(iter),
                index_(index)
            {
            }

            collection_iterator& operator++()
            {
                ++iter_;
                ++index_;
                return *this;
            }

            collection_iterator operator++(int)
            {
                auto old = *this;
                ++iter_;
                ++index_;
                return old;
            }

            T const* operator->() const
            {
                return &*iter_;
            }

            T const& operator*() const
            {
                return *iter_;
            }

            bool operator==(collection_iterator const& rhs) const
            {
                return iter_ == rhs.iter_;
            }

            bool operator!=(collection_iterator const& rhs) const
            {
                return iter_ != rhs.iter_;
            }

            collection_iterator operator+(size_t amount) const
            {
                return collection_iterator(iter_ + amount, index_ + amount);
            }

            typename std::vector<T>::const_iterator iter() const
            {
                return iter_;
            }

            size_t index() const
            {
                return index_;
            }

        private:
            typename std::vector<T>::const_iterator iter_;
            size_t index_;
        };
    }

    template <typename T>
    class reader_t
    {
    public:
        using mutex_t = typename detail::collection_deferred<T>::mutex_t;
        using lock_t = typename detail::collection_deferred<T>::lock_t;
        using const_iterator = typename std::vector<T>::const_iterator;

        reader_t(btl::shared<detail::collection_deferred<T>> deferred) :
            deferred_(std::move(deferred)),
            lock_(deferred_->mutex_)
        {
        }

        size_t size() const
        {
            return deferred_->values_.size();
        }

        bool empty() const
        {
            return deferred_->values_.empty();
        }

        T const& operator[](size_t index) const
        {
            return deferred_->values_[index];
        }

        const_iterator begin() const
        {
            return deferred_->values_.begin();
        }

        const_iterator end() const
        {
            return deferred_->values_.end();
        }

    private:
        btl::shared<detail::collection_deferred<T>> deferred_;
        lock_t lock_;
    };

    template <typename T>
    class writer_t
    {
    public:
        //using iterator = typename std::vector<T>::iterator;
        using const_iterator = detail::collection_iterator<T>;
        using mutex_t = typename detail::collection_deferred<T>::mutex_t;
        using lock_t = typename detail::collection_deferred<T>::lock_t;

        writer_t(btl::shared<detail::collection_deferred<T>> deferred) :
            deferred_(std::move(deferred)),
            lock_(deferred_->mutex_)
        {
        }

        writer_t(writer_t&&) noexcept = default;
        writer_t& operator=(writer_t&&) noexcept = default;

        ~writer_t()
        {
            for (auto&& v : event_.added)
                v.second = &deferred_->values_[v.first];
            for (auto&& v : event_.updated)
                v.second = &deferred_->values_[v.first];

            std::sort(event_.removed.begin(), event_.removed.end());
            std::sort(event_.added.begin(), event_.added.end());
            std::sort(event_.updated.begin(), event_.updated.end());

            deferred_->on_changed_(event_);
        }

        const_iterator begin()
        {
            return const_iterator(deferred_->values_.begin(), 0);
        }

        const_iterator end()
        {
            return const_iterator(deferred_->values_.end(),
                    deferred_->values_.size());
        }

        void push_back(T const& t)
        {
            auto i = deferred_->values_.size();
            deferred_->values_.push_back(t);
            event_.added.push_back(std::make_pair(i, nullptr));
        }

        void push_back(T&& t)
        {
            auto i = deferred_->values_.size();
            deferred_->values_.push_back(std::move(t));
            event_.added.push_back(std::make_pair(i, nullptr));
        }

        static void incrementAfter(
                std::vector<std::pair<size_t, T const*>>& vec, size_t index)
        {
            auto i = vec.begin();
            while (i != vec.end() && i->first != index)
                ++i;

            while (i != vec.end())
            {
                ++i->first;
                ++i;
            }
        }

        void insert(const_iterator pos, T const& t)
        {
            incrementAfter(event_.updated, pos.index());
            incrementAfter(event_.added, pos.index());

            deferred_->values_.insert(pos.iter(), t);
            event_.added.push_back(std::make_pair(pos.index(), nullptr));
        }

        void insert(const_iterator pos, T&& t)
        {
            incrementAfter(event_.updated, pos.index());
            incrementAfter(event_.added, pos.index());

            deferred_->values_.insert(pos.iter(), std::move(t));
            event_.added.push_back(std::make_pair(pos.index(), nullptr));
        }

        void erase_index(std::vector<size_t>& vec, const_iterator i)
        {
            auto first = vec.begin();
            auto result = first;
            while (first != vec.end())
            {
                if (*first != i.index())
                {
                    if (*first > i.index())
                        --(*first);
                    *result = std::move(*first);
                    ++result;
                }
                ++first;
            }

            vec.erase(result, vec.end());
        }

        void erase_index(std::vector<std::pair<size_t, T const*>>& vec,
                const_iterator i)
        {
            auto first = vec.begin();
            auto result = first;
            size_t diff = 0;
            while (first != vec.end())
            {
                if (first->first != i.index())
                {
                    if (first->first > i.index())
                        --first->first;
                    *result = std::move(*first);
                    result->first -= diff;
                    ++result;
                }
                else
                    ++diff;
                ++first;
            }

            vec.erase(result, vec.end());
        }

        void erase(const_iterator i)
        {
            erase_index(event_.added, i);
            erase_index(event_.updated, i);
            //erase_index(event_.removed, i);

            auto index = i.index();
            for (auto const& v : event_.removed)
            {
                if (v <= index)
                    ++index;
            }

            event_.removed.push_back(index);
            deferred_->values_.erase(i.iter());
        }

        size_t size() const
        {
            return deferred_->values_.size();
        }

        detail::collection_ref<T> operator[](size_t index)
        {
            return detail::collection_ref<T>(event_,
                    deferred_->values_[index], index);
        }

    private:
        btl::shared<detail::collection_deferred<T>> deferred_;
        collection_event<T> event_;
        lock_t lock_;
    };

    template <typename T>
    class collection
    {
    public:
        using size_t = typename std::vector<T>::size_type;
        using mutex_t = SpinLock;
        using lock_t = std::unique_lock<mutex_t>;

        collection() :
            deferred_(std::make_shared<detail::collection_deferred<T>>())
        {
        }

        collection(collection const&) = default;
        collection(collection&&) noexcept = default;

        collection& operator=(collection const&) = default;
        collection& operator=(collection&&) noexcept = default;

        connection on_changed(
                std::function<void(collection_event<T> const&)> cb) const
        {
            return deferred_->on_changed_.observe(cb);
        }

        reader_t<T> reader() const
        {
            return reader_t<T>(deferred_);
        }

        writer_t<T> writer()
        {
            return writer_t<T>(deferred_);
        }

    private:
        btl::shared<detail::collection_deferred<T>> deferred_;
    };

    struct AcceptAny
    {
        template <typename... Args>
        void operator()(Args&&...)
        {
        }
    };

    /*
    template <typename T>
    struct CollectionValueType
    {
        using type = typename std::decay<decltype(
                btl::reader(std::declval<T>())[0])>::type;
    };
    */

    template <typename T>
    using CollectionValueType =
        std::decay_t<decltype(
                btl::reader(std::declval<std::decay_t<T>>())[0]
                )
        >;

    static_assert(std::is_same<int,
            CollectionValueType<collection<int>&>
            >::value, "");

    template <typename T, typename = void>
    struct IsCollection : std::false_type {};

    template <typename T>
    struct IsCollection<T, btl::void_t
    <
        decltype(btl::reader(std::declval<std::decay_t<T>>())[0]),
        decltype(btl::observe(std::declval<std::decay_t<T>>(),
            std::function<
                void(collection_event<CollectionValueType<T>> const&)
            >())
        )
    >>: std::true_type {};

    template <typename T, typename T2, typename = void>
    struct IsCollectionType : std::false_type {};

    template <typename T, typename T2>
    struct IsCollectionType<T, T2, btl::void_t
    <
        decltype(btl::reader(std::declval<std::decay_t<T>>())[0]),
        decltype(btl::observe(std::declval<std::decay_t<T>>(),
            std::function<void(collection_event<
                CollectionValueType<T>
                > const&)
            >())),
        decltype(std::declval<T2>() = btl::reader(std::declval<T>())[0])
    >>: std::true_type {};

    static_assert(IsCollection<collection<int>>::value, "");
}

