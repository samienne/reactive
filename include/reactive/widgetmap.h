#pragma once

#include "widgetsetters.h"
#include "widgetgetters.h"
#include "widget.h"

#include <btl/typelist.h>
#include <btl/tuplemap.h>

#include <functional>
#include <tuple>

namespace reactive
{
    using WidgetMap = std::function<Widget(Widget)>;

    template <typename T, typename = void>
    struct HasResultOf : std::false_type {};

    template <typename T, typename... Us>
    struct HasResultOf<T(Us...), btl::void_t<std::result_of_t<T(Us...)>>>
    : std::true_type {};

    template <typename T>
    using IsWidgetMap = btl::All<
        std::is_copy_constructible<std::decay_t<T>>,
        btl::IsClonable<std::decay_t<T>>,
        HasResultOf<std::decay_t<T>(WidgetBase<>)>
        >;

    template <typename TFunc, typename Tuple>
    struct IsWidgetMapWithTuple : std::false_type {};

    template <typename TFunc, typename... Ts>
    struct IsWidgetMapWithTuple<TFunc, std::tuple<Ts...>>
    : IsWidgetMap<TFunc>
    {
    };

    template <typename TFunc>
    struct WidgetValueProvider;

    namespace detail
    {
        template <typename TWidget>
        auto doShare(TWidget widget, btl::TypeList<>)
        {
            return std::move(widget);
        }

        template <typename TWidget, typename T, typename... Ts>
        auto doShare(TWidget widget, btl::TypeList<T, Ts...>)
        {
            auto s = signal::share(get<T>(widget));
            auto newWidget = doShare(
                    std::move(widget),
                    btl::TypeList<Ts...>()
                    );

            return set(std::move(newWidget), std::move(s));
        }

        template <typename T>
        struct ToTypeList
        {
            using type = btl::TypeList<T>;
        };

        template <typename... Ts>
        struct ToTypeList<std::tuple<Ts...>>
        {
            using type = btl::TypeList<Ts...>;
        };

        template <typename T>
        using ToTypeListT = typename ToTypeList<T>::type;

        template <template<typename U> class T, typename... Ts>
        struct TupleMap {};

        template <template<typename U> class T, typename... Ts>
        struct TupleMap<T, std::tuple<Ts...>>
        {
            using type = std::tuple<T<Ts>...>;
        };

        template <template<typename U> class T, typename... Ts>
        using TupleMapT = typename TupleMap<T, Ts...>::type;


    } // detail

    template <typename TFunc, typename TTuple, typename... Ts>
    struct WidgetMapper
    {
        template <typename TWidget, typename =
            std::enable_if_t<IsWidget<TWidget>::value>
            >
        auto operator()(TWidget widget) -> decltype(auto)
        {
            // ReturnTypes contains all types returned by TFunc
            using ReturnTypes = detail::ToTypeListT<
                decltype(
                    std::apply(
                        std::declval<TFunc>(),
                        std::tuple_cat(
                            std::make_tuple(get<Ts>(widget).evaluate()...),
                            std::declval<
                                detail::TupleMapT<
                                    SignalType,
                                    std::decay_t<TTuple>
                                    >
                                >()
                            )
                        )
                    )
                >;

            // This will map types to dependent types
            // (e.g. avg::Vector2f -> avg::Obb)
            using Types = btl::MapListType<DependentType, ReturnTypes>;

            using UniqueTypes = btl::UniqueType<Types>;

            // Types that are listed multiple times
            using MultiTypes = btl::DifferenceType<
                ReturnTypes,
                UniqueTypes
            >;

            // Types that should be be turned into shared signals
            using ShareTypes = btl::UniqueType<
                btl::ConcatType<
                    MultiTypes,
                    btl::DifferenceType<
                        UniqueTypes,
                        btl::TypeList<Ts...>
                    >
                >
            >;

            // Share selected signals
            auto w = detail::doShare(
                        std::move(widget),
                        ShareTypes()
                );

            auto value = std::apply(
                    [func=std::move(func)](auto&&... us)
                    {
                        return signal::map(
                            std::move(func),
                            std::forward<decltype(us)>(us)...
                            );
                    },
                    std::tuple_cat(
                        std::make_tuple(get<Ts>(w)...),
                        btl::clone(*tuple)
                        )
                    );

            auto ret = set(std::move(w), std::move(value));

            static_assert(IsWidget<decltype(ret)>::value, "");

            return ret;
        }

        std::decay_t<TFunc> func;
        btl::CloneOnCopy<std::decay_t<TTuple>> tuple;
    };

    template <typename TFunc>
    struct WidgetMapWrapper;

    template <typename TFunc, typename = std::enable_if_t<
        IsWidgetMap<TFunc>::value
        >
    >
    auto widgetMap(TFunc func)
    {
        auto r = WidgetMapWrapper<std::decay_t<TFunc>>{
            std::move(func)
        };
        static_assert(IsWidgetMap<decltype(r)>::value, "");
        return r;
    }

    template <typename TFunc>
    struct WidgetMapWrapper
    {
        std::decay_t<TFunc> func;

        template <typename TWidget, typename = std::enable_if_t<
            IsWidget<TWidget>::value
        >>
        auto operator()(TWidget widget)
        {
            return func(std::move(widget));
        }

        WidgetMapWrapper clone() const
        {
            return *this;
        }

        template <typename UFunc>
        auto operator|(WidgetMapWrapper<UFunc>&& rhs) &&
        {
            return widgetMap([self=std::move(*this), rhs=std::move(rhs)]
                    (auto widget) mutable
                    {
                        return std::move(widget) | self | rhs;
                    });
        }
    };

    template <typename... Ts, typename TFunc, typename... Us, typename =
        btl::void_t<
            decltype(std::declval<TFunc>()(
                        std::declval<typename Ts::Type>()...,
                        std::declval<SignalType<Us>>()...
                        )
                    )
        >>
    auto makeWidgetMap(TFunc&& func, Us&&... us)
    {
        auto mapper = WidgetMapper<
            std::decay_t<TFunc>,
            std::tuple<std::decay_t<Us>...>,
            Ts...>{
                std::forward<TFunc>(func),
                std::make_tuple(std::forward<Us>(us)...)
            };

        return widgetMap(std::move(mapper));
    }

    template <typename... Ts, typename = std::enable_if_t<
        btl::All<IsWidgetMap<Ts>...>::value
        >>
    auto combineWidgetMaps(Ts&&... maps)
    {
        return widgetMap([maps=std::make_tuple(std::forward<Ts>(maps)...)]
            (auto widget)
            {
                return btl::tuple_reduce(std::move(widget), maps,
                        [](auto widget, auto map)
                        {
                            return map(std::move(widget));
                        });
            });
    }
}

