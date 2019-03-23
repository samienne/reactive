#pragma once

#include "connection.h"

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

        /*
        T& operator*()
        {
            return **iter_;
        }
        */

        T const& operator*() const
        {
            return **iter_;
        }

        /*
        T& operator->()
        {
            return **iter_;
        }
        */

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
            callbacks_(std::make_shared<Callbacks>())
        {
        }

        Collection(Collection const& rhs) = delete;
        Collection(Collection&& rhs) = delete;

        Collection& operator=(Collection const& rhs) = delete;
        Collection& operator=(Collection&& rhs) = delete;

        using Iterator = CollectionIterator<T, typename StorageType::iterator>;
        using ConstIterator = CollectionConstIterator<T,
              typename StorageType::const_iterator>;
        using ReverseIterator = CollectionIterator<T,
              typename StorageType::reverse_iterator>;
        using ConstReverseIterator = CollectionConstIterator<T,
              typename StorageType::const_reverse_iterator>;

        using IdType = size_t;

        using InsertCallback = std::function<void(IdType key, T const& value)>;
        using UpdateCallback = std::function<void(IdType key, T const& value)>;
        using EraseCallback = std::function<void(IdType key)>;

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
            auto i = data_.insert(position.iter_,
                    CollectionValue<T>(std::move(value)));

            for (auto const& cb : callbacks_->insertCallbacks)
            {
                //cb.second(reinterpret_cast<size_t>(&**i), **i);
                cb.second(reinterpret_cast<size_t>(i->ptr()), **i);
            }
        }

        void update(Iterator position, T value)
        {
            assert(position != end());

            auto i = position.iter_;
            **i = value;

            for (auto const& cb : callbacks_->updateCallbacks)
            {
                cb.second(reinterpret_cast<size_t>(i->ptr()), **i);
            }
        }

        void erase(Iterator position)
        {
            assert(position != end());

            auto i = position.iter_;
            size_t id = reinterpret_cast<size_t>(i->ptr());
            data_.erase(i);

            for (auto const& cb : callbacks_->eraseCallbacks)
            {
                cb.second(id);
            }
        }

        Iterator begin()
        {
            return Iterator(data_.begin());
        }

        ConstIterator begin() const
        {
            return ConstIterator(data_.begin());
        }

        ReverseIterator rbegin()
        {
            return ReverseIterator(data_.rbegin());
        }

        ConstReverseIterator rbegin() const
        {
            return ConstReverseIterator(data_.rbegin());
        }

        Iterator end()
        {
            return Iterator(data_.end());
        }

        ConstIterator end() const
        {
            return ConstIterator(data_.end());
        }

        ReverseIterator rend()
        {
            return ReverseIterator(data_.rend());
        }

        ConstReverseIterator rend() const
        {
            return ConstReverseIterator(data_.rend());
        }

        size_t size() const
        {
            return data_.size();
        }

        Connection onInsert(InsertCallback callback)
        {
            size_t id = callbacks_->nextId++;
            callbacks_->insertCallbacks.emplace_back(id, std::move(callback));
            std::weak_ptr<Callbacks> callbacks = callbacks_;

            return Connection::on_disconnect(
                    [callbacks=std::move(callbacks), id]()
                    {
                        if (auto p = callbacks.lock())
                        {
                            eraseFrom(id, p->insertCallbacks);
                        }
                    });
        }

        Connection onUpdate(UpdateCallback callback)
        {
            size_t id = callbacks_->nextId++;
            callbacks_->updateCallbacks.emplace_back(
                    id, std::move(callback));
            std::weak_ptr<Callbacks> callbacks = callbacks_;

            return Connection::on_disconnect(
                    [callbacks=std::move(callbacks), id]()
                    {
                        if (auto p = callbacks.lock())
                        {
                            eraseFrom(id, p->updateCallbacks);
                        }
                    });
        }

        Connection onErase(EraseCallback callback)
        {
            size_t id = callbacks_->nextId++;
            callbacks_->eraseCallbacks.emplace_back(
                    id, std::move(callback));
            std::weak_ptr<Callbacks> callbacks = callbacks_;

            return Connection::on_disconnect(
                    [callbacks=std::move(callbacks), id]()
                    {
                        if (auto p = callbacks.lock())
                        {
                            eraseFrom(id, p->eraseCallbacks);
                        }
                    });
        }

    private:
        template <typename U>
        static void eraseFrom(size_t id, U& vec)
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
        struct Callbacks
        {
            size_t nextId = 1;
            std::vector<std::pair<size_t, InsertCallback>> insertCallbacks;
            std::vector<std::pair<size_t, UpdateCallback>> updateCallbacks;
            std::vector<std::pair<size_t, EraseCallback>> eraseCallbacks;
        };

        StorageType data_;
        std::shared_ptr<Callbacks> callbacks_;
    };
} // namespace reactive

