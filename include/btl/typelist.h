#pragma once

#include <string>
#include <type_traits>

namespace btl
{
    template <typename...>
    struct TypeList {};

    template <typename T>
    struct Head {};

    template <typename T, typename... Ts>
    struct Head<TypeList<T, Ts...>>
    {
        using type = T;
    };

    template <typename T>
    using HeadType = typename Head<T>::type;

    template <typename T>
    struct Tail {};

    template <typename T, typename... Ts>
    struct Tail<TypeList<T, Ts...>>
    {
        using type = TypeList<Ts...>;
    };

    template <typename T>
    using TailType = typename Tail<T>::type;

    template <typename T, typename U>
    struct Concat {};

    template <typename... Ts, typename... Us>
    struct Concat<TypeList<Ts...>, TypeList<Us...>>
    {
        using type = TypeList<Ts..., Us...>;
    };

    template <typename T, typename U>
    using ConcatType = typename Concat<T, U>::type;

    template <template<typename> class TPredicate, typename T>
    struct Filter {};

    template <template<typename> class TPredicate>
    struct Filter<TPredicate, TypeList<>> { using type = TypeList<>; };

    template <template<typename> class TPredicate, typename T, typename... Ts>
    struct Filter<TPredicate, TypeList<T, Ts...>> : std::conditional<
        TPredicate<T>::value,
        ConcatType<TypeList<T>, typename Filter<TPredicate, TypeList<Ts...>>::type>,
        typename Filter<TPredicate, TypeList<Ts...>>::type
    > {};

    template <template<typename> class TMap, typename T>
    struct MapList {};

    template <template<typename> class TMap>
    struct MapList<TMap, TypeList<>>
    {
        using type = TypeList<>;
    };

    template <template<typename> class TMap, typename... Ts>
    struct MapList<TMap, TypeList<Ts...>>
    {
        using type = TypeList<TMap<Ts>...>;
    };

    template <template<typename> class TMap, typename T>
    using MapListType = typename MapList<TMap, T>::type;

    template <typename T, typename U>
    struct Contains {};

    template <typename T>
    struct Contains<T, TypeList<>> : std::false_type {};

    template <typename T, typename U, typename... Us>
    struct Contains<T, TypeList<U, Us...>> : std::conditional_t<
        std::is_same<T, U>::value,
        std::true_type,
        Contains<T, TypeList<Us...>>
    > {};

    template <typename T>
    struct Unique {};

    template <>
    struct Unique<TypeList<>>
    {
        using type = TypeList<>;
    };

    template <typename T, typename... Ts>
    struct Unique<TypeList<T, Ts...>> : std::conditional_t<
        Contains<T, TypeList<Ts...>>::value,
        Unique<TypeList<Ts...>>,
        Concat<TypeList<T>, typename Unique<TypeList<Ts...>>::type>
        > {};

    template <typename T>
    using UniqueType = typename Unique<T>::type;

    template <typename T, typename U>
    struct Intersection {};

    template <typename... Us>
    struct Intersection<TypeList<>, TypeList<Us...>>
    {
        using type = TypeList<>;
    };

    template <typename T, typename... Ts, typename... Us>
    struct Intersection<TypeList<T, Ts...>, TypeList<Us...>> :
    std::conditional_t<
        Contains<T, TypeList<Us...>>::value,
        Concat<
            TypeList<T>,
            typename Intersection<TypeList<Ts...>, TypeList<Us...>>::type
        >,
        Intersection<TypeList<Ts...>, TypeList<Us...>>
    > {};

    template <typename T, typename U>
    using IntersectionType = typename Unique<typename Intersection<T, U>::type>::type;

    template <typename T, typename U>
    struct Difference {};

    template <typename... Us>
    struct Difference<TypeList<>, TypeList<Us...>>
    {
        using type = TypeList<>;
    };

    template <typename T, typename... Ts, typename... Us>
    struct Difference<TypeList<T, Ts...>, TypeList<Us...>> :
    std::conditional_t<
        !Contains<T, TypeList<Us...>>::value,
        Concat<
            TypeList<T>,
            typename Difference<TypeList<Ts...>, TypeList<Us...>>::type
        >,
        Difference<TypeList<Ts...>, TypeList<Us...>>
    > {};

    template <typename T, typename U>
    using DifferenceType = typename Unique<typename Difference<T, U>::type>::type;

    namespace typelist
    {
        template <typename... Ts>
        auto unique(TypeList<Ts...>&&)
        {
            return typename Unique<TypeList<Ts...>>::type();
        }
    }
} // btl

