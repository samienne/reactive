#pragma once

#include "connection.h"

#include <btl/spinlock.h>

#include <algorithm>
#include <vector>
#include <utility>
#include <memory>
#include <functional>
#include <iostream>

namespace reactive
{
    template <typename T>
    class CollectionValue
    {
    public:
        explicit CollectionValue(T value) :
            value_(std::make_unique<T>(std::move(value)))
        {
        };

        CollectionValue(CollectionValue const& rhs) :
            value_(std::make_unique(*rhs.value_))
        {
        }

        CollectionValue(CollectionValue&& rhs) noexcept:
            value_(std::move(rhs.value_))
        {
        }

        CollectionValue& operator=(CollectionValue const& rhs)
        {
            value_ = std::make_unique<T>(rhs.value_);
            return *this;
        }

        CollectionValue& operator=(CollectionValue&& rhs)
        {
            value_ = std::move(rhs.value_);
            return *this;
        }

        T& operator*()
        {
            return *value_;
        }

        T const& operator*() const
        {
            return *value_;
        }

        T* operator->()
        {
            return value_.get();
        }

        T const* operator->() const
        {
            return value_.get();
        }

        T const* ptr() const
        {
            return value_.get();
        }

    private:
        std::unique_ptr<T> value_;
    };

    template <typename T, typename TIter>
    class CollectionConstIterator
    {
    public:
        using value_type = T;

        CollectionConstIterator(TIter iter) :
            iter_(std::move(iter))
        {
        }

        T const& operator*() const
        {
            return **iter_;
        }

        T const* operator->() const
        {
            return iter_->ptr();
        }

        CollectionConstIterator& operator++()
        {
            ++iter_;
            return *this;
        }

        CollectionConstIterator operator++(int)
        {
            return CollectionConstIterator(iter_++);
        }

        CollectionConstIterator operator+(int amount) const
        {
            return CollectionConstIterator(iter_ + amount);
        }

        auto operator-(CollectionConstIterator const& rhs) const
        {
            return iter_ - rhs.iter_;
        }

        bool operator==(CollectionConstIterator const& rhs) const
        {
            return iter_ == rhs.iter_;
        }

        bool operator!=(CollectionConstIterator const& rhs) const
        {
            return iter_ != rhs.iter_;
        }

        bool operator<(CollectionConstIterator const& rhs) const
        {
            return iter_ < rhs.iter_;
        }

        bool operator>(CollectionConstIterator const& rhs) const
        {
            return iter_ > rhs.iter_;
        }

        size_t getId() const
        {
            return reinterpret_cast<size_t>(iter_->ptr());
        }

    protected:
        TIter iter_;
    };

    template <typename T, typename TIter>
    class CollectionIterator : public CollectionConstIterator<T, TIter>
    {
    public:
        using value_type = T;
        using difference_type = typename TIter::difference_type;
        using pointer = typename TIter::pointer;
        using reference = typename TIter::reference;
        using iterator_category = typename TIter::iterator_category;

        CollectionIterator(TIter iter) :
            CollectionConstIterator<T, TIter>(std::move(iter))
        {
        }

        T const& operator*() const
        {
            return **iter_;
        }

        T const* operator->() const
        {
            return iter_->get();
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

        CollectionIterator operator+(int amount) const
        {
            return CollectionIterator(iter_ + amount);
        }

    protected:
        template <typename U>
        friend class Collection;

        using CollectionConstIterator<T, TIter>::iter_;
    };

    template <typename T>
    class Collection
    {
        using StorageType = std::vector<CollectionValue<T>>;

    public:
        Collection() :
            control_(std::make_shared<ControlBlock>())
        {
        }

        Collection(Collection const& rhs) = default;
        Collection(Collection&& rhs) noexcept = default;

        Collection& operator=(Collection const& rhs) = default;
        Collection& operator=(Collection&& rhs)  noexcept= default;

        using Iterator = CollectionIterator<T, typename StorageType::iterator>;
        using ConstIterator = CollectionConstIterator<T,
              typename StorageType::const_iterator>;
        using ReverseIterator = CollectionIterator<T,
              typename StorageType::reverse_iterator>;
        using ConstReverseIterator = CollectionConstIterator<T,
              typename StorageType::const_reverse_iterator>;
        using MutexType = btl::SpinLock;
        using LockType = std::unique_lock<MutexType>;

        class ConstRange
        {
        public:
            ConstRange(LockType lock, Collection const& collection) :
                lock_(std::move(lock)),
                collection_(collection)
            {
            }

            ConstIterator begin() const
            {
                return ConstIterator(collection_.control_->data.begin());
            }

            ConstReverseIterator rbegin() const
            {
                return ConstReverseIterator(collection_.control_->data.rbegin());
            }

            ConstIterator end() const
            {
                return ConstIterator(collection_.control_->data.end());
            }

            ConstReverseIterator rend() const
            {
                return ConstReverseIterator(collection_.control_->data.rend());
            }

            size_t size() const
            {
                return collection_.control_->data.size();
            }

            ConstIterator findId(size_t id) const
            {
                for (auto i = begin(); i != end(); ++i)
                    if (i.getId() == id)
                        return i;

                return end();
            }

            T const& operator[](int index) const
            {
                return *(begin() + index);
            }

        protected:
            LockType lock_;
            Collection const& collection_;
        };

        class Range : public ConstRange
        {
            //using ConstRange::collection_;
            Collection& collection_;

        public:
            Range(LockType lock, Collection& collection) :
                ConstRange(std::move(lock), collection),
                collection_(collection)
            {
            }

            Iterator begin()
            {
                return Iterator(collection_.control_->data.begin());
            }

            ReverseIterator rbegin()
            {
                return ReverseIterator(collection_.control_->data.rbegin());
            }

            Iterator end()
            {
                return Iterator(collection_.control_->data.end());
            }

            ReverseIterator rend()
            {
                return ReverseIterator(collection_.control_->data.rend());
            }

            void pushBack(T value)
            {
                insert(end(), std::move(value));
            }

            void pushFront(T value)
            {
                insert(begin(), std::move(value));
            }

            Iterator insert(Iterator position, T value)
            {
                auto i = collection_.control_->data.insert(position.iter_,
                        CollectionValue<T>(std::move(value)));

                auto index = std::distance(collection_.control_->data.begin(), i);

                for (auto const& cb : collection_.control_->insertCallbacks)
                {
                    cb.second(
                            reinterpret_cast<size_t>(i->ptr()),
                            static_cast<int>(index),
                            **i
                            );
                }

                return Iterator(i);
            }

            void update(Iterator position, T value)
            {
                assert(position != end());

                auto i = position.iter_;
                **i = value;

                for (auto const& cb : collection_.control_->updateCallbacks)
                {
                    cb.second(
                            reinterpret_cast<size_t>(i->ptr()),
                            static_cast<int>(
                                std::distance(collection_.control_->data.begin(),
                                    i)),
                            **i
                            );
                }
            }

            void erase(Iterator position)
            {
                assert(position != end());

                auto i = position.iter_;
                size_t id = reinterpret_cast<size_t>(i->ptr());
                collection_.control_->data.erase(i);

                for (auto const& cb : collection_.control_->eraseCallbacks)
                {
                    cb.second(id);
                }
            }

            void swap(Iterator a, Iterator b)
            {
                assert(a != end() && b != end());

                size_t id1 = a.getId();
                size_t id2 = b.getId();
                int index1 = static_cast<int>(std::distance(begin(), a));
                int index2 = static_cast<int>(std::distance(begin(), b));

                std::swap(*a.iter_, *b.iter_);

                for (auto const& cb : collection_.control_->swapCallbacks)
                {
                    cb.second(id1, index1, id2, index2);
                }
            }

            void move(Iterator from, Iterator to)
            {
                assert(from != end());

                size_t id = from.getId();
                auto newIndex = std::distance(begin(), to);

                if (to.iter_ < from.iter_)
                {
                    // from.iter cannot be end() so +1 is ok.
                    std::rotate(to.iter_, from.iter_, from.iter_+1);
                }
                else if (from.iter_ < to.iter_)
                {
                    auto e = to.iter_;
                    if (to != end())
                        ++e;

                    std::rotate(from.iter_, from.iter_ + 1, e);
                }
                else
                    return;

                for (auto const& cb : collection_.control_->moveCallbacks)
                {
                    cb.second(id, static_cast<int>(newIndex));
                }
            }

            Iterator findId(size_t id)
            {
                for (auto i = begin(); i != end(); ++i)
                    if (i.getId() == id)
                        return i;

                return end();
            }

            void eraseWithId(size_t id)
            {
                auto i = findId(id);
                if (i == end())
                    return;

                erase(i);
            }

            template <typename TCompare = std::less<T>>
            void sort(TCompare&& compare = std::less<T>())
            {
                std::sort(
                        collection_.control_->data.begin(),
                        collection_.control_->data.end(),
                        [&compare](auto const& a, auto const& b)
                        {
                            return compare(*a, *b);
                        });

                std::vector<std::pair<size_t, T>> result;
                result.reserve(collection_.control_->data.size());

                for (auto i = begin(); i != end(); ++i)
                {
                    result.emplace_back(i.getId(), *i);
                }

                if (collection_.control_->refreshCallbacks.size() == 1)
                {
                    for (auto const& cb : collection_.control_->refreshCallbacks)
                        cb.second(std::move(result));
                }
                else
                {
                    for (auto const& cb : collection_.control_->refreshCallbacks)
                        cb.second(result);
                }
            }
        };

        using IdType = size_t;

        using InsertCallback = std::function<void(IdType key, int index,
                T const& value)>;
        using UpdateCallback = std::function<void(IdType key, int index,
                T const& value)>;
        using EraseCallback = std::function<void(IdType key)>;
        using SwapCallback = std::function<void(IdType id1, int index1,
                IdType id2, int index2)>;
        using MoveCallback = std::function<void(IdType key, int index)>;
        using RefreshCallback = std::function<void(
                std::vector<std::pair<size_t, T>>)>;

        Range rangeLock()
        {
            return Range(LockType(control_->mutex), *this);
        }

        ConstRange rangeLock() const
        {
            return ConstRange(LockType(control_->mutex), *this);
        }

        ConstRange crangeLock() const
        {
            return ConstRange(LockType(control_->mutex), *this);
        }

        Connection onInsert(InsertCallback callback)
        {
            return addCallbackTo(control_->insertCallbacks, std::move(callback));
        }

        Connection onUpdate(UpdateCallback callback)
        {
            return addCallbackTo(control_->updateCallbacks, std::move(callback));
        }

        Connection onErase(EraseCallback callback)
        {
            return addCallbackTo(control_->eraseCallbacks, std::move(callback));
        }

        Connection onSwap(SwapCallback callback)
        {
            return addCallbackTo(control_->swapCallbacks, std::move(callback));
        }

        Connection onMove(MoveCallback callback)
        {
            return addCallbackTo(control_->moveCallbacks, std::move(callback));
        }

        Connection onRefresh(RefreshCallback callback)
        {
            return addCallbackTo(control_->refreshCallbacks, std::move(callback));
        }

    private:
        // The vec needs to inside the control block of the collection.
        template <typename U, typename V>
        Connection addCallbackTo(U& vec, V&& callback)
        {
            LockType lock(control_->mutex);

            size_t id = control_->nextId++;
            vec.emplace_back(id, std::forward<V>(callback));
            std::weak_ptr<ControlBlock> control = control_.ptr();

            return Connection::on_disconnect(
                    [control=std::move(control), &vec, id]() mutable
                    {
                        if (auto p = control.lock())
                        {
                            LockType l(p->mutex);
                            eraseCallbackFrom(l, id, vec);
                        }
                    });
        }

        template <typename U>
        static void eraseCallbackFrom(LockType&, size_t id, U& vec)
        {
            for (auto i = vec.begin(); i!= vec.end(); ++i)
            {
                if (i->first == id)
                {
                    vec.erase(i);
                    return;
                }
            }
        }

    private:
        struct ControlBlock
        {
            mutable btl::SpinLock mutex;
            StorageType data;
            size_t nextId = 1;
            std::vector<std::pair<size_t, InsertCallback>> insertCallbacks;
            std::vector<std::pair<size_t, UpdateCallback>> updateCallbacks;
            std::vector<std::pair<size_t, EraseCallback>> eraseCallbacks;
            std::vector<std::pair<size_t, SwapCallback>> swapCallbacks;
            std::vector<std::pair<size_t, MoveCallback>> moveCallbacks;
            std::vector<std::pair<size_t, RefreshCallback>> refreshCallbacks;
        };

        btl::shared<ControlBlock> control_;
    };
} // namespace reactive

#if 0
namespace std
{
    template <typename T>
    void iter_swap(typename reactive::Collection<T>::Iterator a,
            typename reactive::Collection<T>::Iterator b)
    {
    }
} // namespace std

#endif
