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

    template <typename T>
    using IsWidgetMap = btl::All<
        std::is_convertible<T, WidgetMap>,
        std::is_copy_constructible<std::decay_t<T>>,
        btl::IsClonable<std::decay_t<T>>
        >;

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
                    btl::apply(
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

            auto value = btl::apply(
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

    template <typename... Ts, typename TFunc, typename... Us, typename =
        btl::void_t<
            decltype(std::declval<TFunc>()(
                        std::declval<typename Ts::Type>()...,
                        std::declval<SignalType<Us>>()...
                        )
                    )
        >>
    auto makeWidgetMap(TFunc&& func, Us&&... us)
        /*-> WidgetMapper<
            std::decay_t<TFunc>,
            std::tuple<std::decay_t<Us>...>,
            Ts...
            >*/
    {
        auto mapper = WidgetMapper<
            std::decay_t<TFunc>,
            std::tuple<std::decay_t<Us>...>,
            Ts...>{
                std::forward<TFunc>(func),
                std::make_tuple(std::forward<Us>(us)...)
            };

        static_assert(IsWidgetMap<decltype(mapper)>::value, "");

        return mapper;
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
    };

    template <typename TFunc, typename = std::enable_if_t<
        IsWidgetMap<TFunc>::value
        >
    >
    auto mapWidget(TFunc func)
    {
        auto r = WidgetMapWrapper<std::decay_t<TFunc>>{
            std::move(func)
        };
        static_assert(IsWidgetMap<decltype(r)>::value, "");
        return r;
    }
}

