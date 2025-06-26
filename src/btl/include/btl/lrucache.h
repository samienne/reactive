#pragma once

#include <map>
#include <list>

namespace btl
{
    template<class Key, class T>
    class LruCache
    {
    public:
        typedef Key KeyType;
        typedef typename std::list<std::pair<Key, T>> ListType;
        typedef typename ListType::iterator ListIterator;
        typedef typename ListType::const_iterator ListConstIterator;
        typedef typename std::map<KeyType, ListIterator> MapType;
        typedef typename MapType::iterator MapIterator;
        typedef typename MapType::const_iterator MapConstIterator;
        typedef ListIterator Iterator;
        typedef ListConstIterator ConstIterator;

        LruCache(size_t size);
        ~LruCache();

        size_t getSize() const;
        size_t size() const;

        Iterator insert(std::pair<KeyType, T>&& value);
        Iterator insert(std::pair<KeyType, T> const& value);

        Iterator find(KeyType const& key);
        ConstIterator find(KeyType const& key) const;

        Iterator begin();
        ConstIterator begin() const;

        Iterator end();
        ConstIterator end() const;

        Iterator rbegin();
        ConstIterator rbegin() const;

        Iterator rend();
        ConstIterator rend() const;

    private:
        MapType map_;
        ListType list_;
        size_t size_;
    };

    template<class Key, class T> LruCache<Key, T>::LruCache(size_t size) :
        size_(size)
    {
    }

    template<class Key, class T> LruCache<Key, T>::~LruCache()
    {
    }

    template<class Key, class T> size_t LruCache<Key, T>::getSize() const
    {
        return map_.size();
    }

    template<class Key, class T> size_t LruCache<Key, T>::size() const
    {
        return map_.size();
    }

    template<class Key, class T> typename LruCache<Key, T>::Iterator
        LruCache<Key, T>::insert(std::pair<KeyType, T>&& value)
    {
        auto j = map_.find(value.first);
        if (j != map_.end())
        {
            j->second->second = value.second;
            return j->second;
        }

        if (size_ == map_.size())
        {
            map_.erase(map_.find(list_.front().first));
            list_.erase(list_.begin());
        }

        auto i = list_.insert(list_.end(), std::move(value));
        map_.insert(std::make_pair(i->first, i));

        return i;
    }

    template<class Key, class T> typename LruCache<Key, T>::Iterator
        LruCache<Key, T>::insert(std::pair<KeyType, T> const& value)
    {
        return insert(std::pair<KeyType, T>(value));
    }

    template<class Key, class T>
    typename LruCache<Key, T>::Iterator LruCache<Key, T>::find(
            KeyType const& key)
    {
        auto i = map_.find(key);
        if (i == map_.end())
            return list_.end();

        list_.splice(list_.end(), list_, i->second);

        return i->second;
    }

    template<class Key, class T>
    typename LruCache<Key, T>::ConstIterator LruCache<Key, T>::find(
            KeyType const& key) const
    {
        return const_cast<LruCache<Key, T>*>(this)->find(key);
    }

    template<class Key, class T>
    typename LruCache<Key, T>::Iterator LruCache<Key, T>::begin()
    {
        return list_.begin();
    }

    template<class Key, class T>
    typename LruCache<Key, T>::ConstIterator LruCache<Key, T>::begin() const
    {
        return list_.begin();
    }

    template<class Key, class T>
    typename LruCache<Key, T>::Iterator LruCache<Key, T>::end()
    {
        return list_.end();
    }

    template<class Key, class T>
    typename LruCache<Key, T>::ConstIterator LruCache<Key, T>::end() const
    {
        return list_.end();
    }

    template<class Key, class T>
    typename LruCache<Key, T>::Iterator LruCache<Key, T>::rbegin()
    {
        return list_.rbegin();
    }

    template<class Key, class T>
    typename LruCache<Key, T>::ConstIterator LruCache<Key, T>::rbegin() const
    {
        return list_.rbegin();
    }

    template<class Key, class T>
    typename LruCache<Key, T>::Iterator LruCache<Key, T>::rend()
    {
        return list_.rend();
    }

    template<class Key, class T>
    typename LruCache<Key, T>::ConstIterator LruCache<Key, T>::rend() const
    {
        return list_.rend();
    }
}

