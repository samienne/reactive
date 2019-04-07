#pragma once

#include "connection.h"

#include <btl/spinlock.h>

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

        protected:
            LockType lock_;
            Collection const& collection_;
        };

        class Range : public ConstRange
        {
            using ConstRange::collection_;

        public:
            Range(LockType lock, Collection const& collection) :
                ConstRange(std::move(lock), collection)
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

            void insert(Iterator position, T value)
            {
                auto i = collection_.control_->data.insert(position.iter_,
                        CollectionValue<T>(std::move(value)));

                for (auto const& cb : collection_.control_->insertCallbacks)
                {
                    cb.second(reinterpret_cast<size_t>(i->ptr()), **i);
                }
            }

            void update(Iterator position, T value)
            {
                assert(position != end());

                auto i = position.iter_;
                **i = value;

                for (auto const& cb : collection_.control_->updateCallbacks)
                {
                    cb.second(reinterpret_cast<size_t>(i->ptr()), **i);
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
        };

        using IdType = size_t;

        using InsertCallback = std::function<void(IdType key, T const& value)>;
        using UpdateCallback = std::function<void(IdType key, T const& value)>;
        using EraseCallback = std::function<void(IdType key)>;

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
            LockType lock(control_->mutex);

            size_t id = control_->nextId++;
            control_->insertCallbacks.emplace_back(id, std::move(callback));
            std::weak_ptr<ControlBlock> control = control_;

            return Connection::on_disconnect(
                    [control=std::move(control), id]()
                    {
                        if (auto p = control.lock())
                        {
                            LockType l(p->mutex);
                            eraseFrom(l, id, p->insertCallbacks);
                        }
                    });
        }

        Connection onUpdate(UpdateCallback callback)
        {
            LockType lock(control_->mutex);

            size_t id = control_->nextId++;
            control_->updateCallbacks.emplace_back(
                    id, std::move(callback));
            std::weak_ptr<ControlBlock> control = control_;

            return Connection::on_disconnect(
                    [control=std::move(control), id]()
                    {
                        if (auto p = control.lock())
                        {
                            LockType l(p->mutex);
                            eraseFrom(l, id, p->updateCallbacks);
                        }
                    });
        }

        Connection onErase(EraseCallback callback)
        {
            LockType lock(control_->mutex);

            size_t id = control_->nextId++;
            control_->eraseCallbacks.emplace_back(
                    id, std::move(callback));
            std::weak_ptr<ControlBlock> control = control_;

            return Connection::on_disconnect(
                    [control=std::move(control), id]()
                    {
                        if (auto p = control.lock())
                        {
                            LockType l(p->mutex);
                            eraseFrom(l, id, p->eraseCallbacks);
                        }
                    });
        }

    private:
        template <typename U>
        static void eraseFrom(LockType&, size_t id, U& vec)
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
            btl::SpinLock mutex;
            StorageType data;
            size_t nextId = 1;
            std::vector<std::pair<size_t, InsertCallback>> insertCallbacks;
            std::vector<std::pair<size_t, UpdateCallback>> updateCallbacks;
            std::vector<std::pair<size_t, EraseCallback>> eraseCallbacks;
        };

        std::shared_ptr<ControlBlock> control_;
    };
} // namespace reactive

