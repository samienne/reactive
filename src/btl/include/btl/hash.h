#pragma once

#include "typetraits.h"

#include <array>
#include <vector>
#include <string>

namespace btl
{
    template <class THash, typename T, size_t S>
    void hash_append(THash& hash, std::array<T, S> const& array)
    {
        for (auto i : array)
            hash_append(hash, i);
    }

    template <class HashAlgorithm, class T>
    inline btl::enable_if_t <
    std::is_floating_point<T>::value
    >
    hash_append(HashAlgorithm& h, T t) noexcept
    {
        if (t == 0)
            t = 0;
        h(&t, sizeof(t));
    }

    template <class T> struct is_contiguously_hashable : std::false_type {};

    template <> struct is_contiguously_hashable<int8_t> : std::true_type {};
    template <> struct is_contiguously_hashable<int16_t> : std::true_type {};
    template <> struct is_contiguously_hashable<int32_t> : std::true_type {};
    template <> struct is_contiguously_hashable<int64_t> : std::true_type {};
    template <> struct is_contiguously_hashable<uint8_t> : std::true_type {};
    template <> struct is_contiguously_hashable<uint16_t> : std::true_type {};
    template <> struct is_contiguously_hashable<uint32_t> : std::true_type {};
    template <> struct is_contiguously_hashable<uint64_t> : std::true_type {};

#ifdef __APPLE__
    template <> struct is_contiguously_hashable<size_t> : std::true_type {};
#endif

    template <typename T> struct is_contiguously_hashable<T*> :
        std::true_type {};

    template <class HashAlgorithm, class T>
    inline btl::enable_if_t
    <
        is_contiguously_hashable<T>::value
    >
    hash_append(HashAlgorithm& h, T const& t) noexcept
    {
        h(&t, sizeof(t));
    }



    template <class HashAlgorithm, class T, class Alloc>
    inline
    btl::enable_if_t
    <
        !is_contiguously_hashable<T>::value
    >
    hash_append(HashAlgorithm& h, std::vector<T, Alloc> const& v) noexcept
    {
        for (auto const& t : v)
            hash_append(h, t);
        hash_append(h, v.size());
    }

    template <class HashAlgorithm, class T, class Alloc>
    inline
    btl::enable_if_t
    <
        is_contiguously_hashable<T>::value
    >
    hash_append(HashAlgorithm& h, std::vector<T, Alloc> const& v) noexcept
    {
        h(v.data(), v.size()*sizeof(T));
        hash_append(h, v.size());
    }

    template <class HashAlgorithm, class CharT, class Traits, class Alloc>
    void hash_append(HashAlgorithm& h,
            std::basic_string<CharT, Traits, Alloc> const& s)
    {
        h(s.data(), s.size());
    }

    template <class HashAlgorithm, class T, class U>
    void
    hash_append (HashAlgorithm& h, std::pair<T, U> const& p) noexcept
    {
        hash_append (h, p.first);
        hash_append (h, p.second);
    }
}

