#pragma once

#if 0
#include "collection.h"
#include "spinlock.h"

#include <mutex>

namespace btl
{
    template <typename TValues>
    class mapped_collection_reader
    {
    public:
        using mutex_t = SpinLock;
        using lock_t = std::unique_lock<mutex_t>;

        mapped_collection_reader(TValues const& values, lock_t&& lock) :
            values_(values),
            lock_(std::move(lock))
        {
        }

        auto operator[](size_t index) const
            -> decltype(std::declval<TValues>()[index])
        {
            return values_[index];
        }

        auto begin() const
            -> decltype(std::declval<TValues const>().begin())
        {
            return values_.begin();
        }

        auto end() const
            -> decltype(std::declval<TValues const>().end())
        {
            return values_.end();
        }

        auto size() const
            -> decltype(std::declval<TValues const>().size())
        {
            return values_.size();
        }

    private:
        TValues const& values_;
        lock_t lock_;
    };

    template <typename TCollection, typename TFunc>
    class mapped_collection
    {
    public:
        using reader_t = decltype(btl::reader(std::declval<TCollection>()));
        using value_t = typename std::decay<
            decltype(std::declval<reader_t>()[0])>::type;
        using mapped_value_t = typename std::decay<decltype(
                std::declval<TFunc>()(std::declval<value_t>(), 0u))>::type;
        using connection_t = decltype(btl::observe(
                    std::declval<TCollection const>(),
                    std::function<void(collection_event<value_t> const&)>()));
        using mutex_t = SpinLock;
        using lock_t = std::unique_lock<mutex_t>;
        using mapped_reader_t =
            mapped_collection_reader<std::vector<mapped_value_t>>;

    public:
        mapped_collection(TCollection&& collection, TFunc&& func) :
            collection_(std::forward<TCollection>(collection)),
            func_(std::forward<TFunc>(func))
        {
            {
                auto r = btl::reader(collection_);
                for (auto&& v : r)
                    values_.push_back(func_(v, values_.size()));
            }

            connection_ = observe(collection_,
                    std::bind(&mapped_collection::update, this,
                        std::placeholders::_1));
        }

        mapped_collection(mapped_collection&& rhs) :
            collection_(rhs.collection_),
            values_(std::move(rhs.values_)),
            func_(std::move(rhs.func_))
        {
            lock_t lock(rhs.mutex_);
            connection_ = observe(collection_,
                    std::bind(&mapped_collection::update, this,
                        std::placeholders::_1));
        }

        mapped_collection(mapped_collection const& rhs) :
            collection_(rhs.collection_),
            values_(rhs.values_),
            func_(rhs.func_)
        {
            lock_t lock(rhs.mutex_);
            connection_ = observe(collection_,
                    std::bind(&mapped_collection::update, this,
                        std::placeholders::_1));
        }

        mapped_collection& operator=(mapped_collection const&) = delete;
        mapped_collection& operator=(mapped_collection&&) = delete;

        mapped_reader_t reader() const
        {
            return mapped_reader_t(values_, lock_t(mutex_));
        }

        template <typename TCallback>
        auto on_changed(TCallback&& cb) const -> connection
        {
            return on_changed_.observe(std::forward<TCallback>(cb));
        }

    private:
        /**
         * expects indices to be ordered
         */
        static void erase_indices(std::vector<mapped_value_t>& vec,
                std::vector<size_t> const& indices)
        {
            if (indices.empty())
                return;
            auto head = vec.begin();
            auto tail = head;
            auto index = indices.begin();
            size_t i = 0;
            while (head != vec.end())
            {
                if (index == indices.end() || i != *index)
                {
                    if (tail != head)
                        *tail = std::move(*head);
                    ++tail;
                }

                while (index != indices.end() && i >= *index)
                    ++index;

                ++i;
                ++head;
            }

            vec.erase(tail, vec.end());
        }

        collection_event<mapped_value_t> translate_collection(
                collection_event<value_t> const& e)
        {
            collection_event<mapped_value_t> result;

            result.updated.reserve(e.updated.size());
            for (auto&& v : e.updated)
                result.updated.push_back(std::make_pair(v.first,
                            &values_[v.first]));

            result.added.reserve(e.added.size());
            for (auto&& v : e.added)
            {
                result.added.push_back(std::make_pair(v.first,
                            &values_[v.first]));
            }

            result.removed = e.removed;

            // todo: handle moved

            return std::move(result);
        }

        void update(collection_event<value_t> const& e)
        {
            lock_t(mutex_);
            erase_indices(values_, e.removed);

            for (auto&& v : e.updated)
                values_[v.first] = func_(*v.second, v.first);

            for (auto&& v : e.added)
                values_.push_back(func_(*v.second, v.first));

            on_changed_(translate_collection(e));
        }

    private:
        typename std::decay<TCollection>::type collection_;
        std::vector<mapped_value_t> values_;
        typename std::decay<TFunc>::type func_;
        connection_t connection_;
        mutable mutex_t mutex_;
        mutable btl::observable<void(collection_event<mapped_value_t> const&)>
            on_changed_;
    };

    template <typename TFunc, typename TCollection,
             typename = typename std::enable_if
                 <
                    IsCollection<TCollection>::value,
                    btl::void_t
                    <
                        decltype(std::declval<typename std::decay<TFunc>::type>()(
                                    std::declval<
                                    typename CollectionValueType<TCollection>::type
                                    >(), size_t(0u)))
                    >
                 >::type>
    auto map_collection(TFunc&& func, TCollection&& collection)
        -> mapped_collection<TCollection, TFunc>
    {
        return mapped_collection<TCollection, TFunc>(
                std::forward<TCollection>(collection),
                std::forward<TFunc>(func));
    }

    static_assert(IsCollection<
            mapped_collection<collection<int>,
            std::function<int(int, size_t)>
            >>::value, "");
}
#endif

