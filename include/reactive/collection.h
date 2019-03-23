#pragma once

#include <vector>
#include <utility>
#include <memory>
#include <functional>

namespace reactive
{
    template <typename T>
    class CollectionValue
    {
    public:
        CollectionValue(T value) :
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

        T& operator->()
        {
            return *value_;
        }

        T const& operator->() const
        {
            return *value_;
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

        T const& operator->() const
        {
            return **iter_;
        }

        CollectionConstIterator& operator++()
        {
            ++iter_;
            return *this;
        }

        CollectionConstIterator operator++(int)
        {
            return CollectionIterator(iter_++);
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

        T& operator*()
        {
            return **iter_;
        }

        T const& operator*() const
        {
            return **iter_;
        }

        T& operator->()
        {
            return **iter_;
        }

        T const& operator->() const
        {
            return **iter_;
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
        using Iterator = CollectionIterator<T, typename StorageType::iterator>;
        using ConstIterator = CollectionConstIterator<T,
              typename StorageType::const_iterator>;
        using ReverseIterator = CollectionIterator<T,
              typename StorageType::reverse_iterator>;
        using ConstReverseIterator = CollectionConstIterator<T,
              typename StorageType::const_reverse_iterator>;

        using IdType = size_t;

        using AddCallback = std::function<void(IdType key, T const& value)>;
        using UpdateCallback = std::function<void(IdType key, T const& value)>;
        using RemoveCallback = std::function<void(IdType key)>;

        void pushBack(T value)
        {
            data_.emplace_back(std::move(value));
        }

        void pushFront(T value)
        {
            data_.insert(data_.begin(), CollectionValue<T>(std::move(value)));
        }

        void insert(Iterator position, T value)
        {
            data_.insert(position.iter_, CollectionValue<T>(std::move(value)));
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

    private:
        StorageType data_;
    };
} // namespace reactive

