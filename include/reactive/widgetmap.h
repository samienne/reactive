#pragma once

#include "widgetvalueprovider.h"
#include "widget.h"

#include <btl/typelist.h>
#include <btl/tuplemap.h>

#include <functional>
#include <tuple>

namespace reactive
{
    using WidgetMap = std::function<Widget(Widget)>;

    template <typename T, typename U, typename = void>
    struct HasResultOfType : std::false_type {};

    template <typename T, typename U, typename V>
    struct HasResultOfType<T(U), V, btl::void_t<std::result_of_t<T(U)>>>
    : IsWidget<std::result_of_t<T(U)>> {};

    template <typename T>
    using IsWidgetMap = btl::All<
        std::is_copy_constructible<std::decay_t<T>>,
        btl::IsClonable<std::decay_t<T>>,
        //std::is_convertible<std::decay_t<T>, WidgetMap>
        HasResultOfType<T(Widget), Widget>
        >;

    template <typename TFunc>
    struct WidgetValueProvider;

    template <typename TFunc>
    auto widgetValueProvider(TFunc&& func);

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
        auto map(WidgetMapWrapper<UFunc>&& wmap) &&
        {
            return widgetMap([func=std::move(func), wmap=std::move(wmap)]
                    (auto widget) mutable
                    {
                        return std::move(wmap)(
                                std::move(func)(
                                    std::move(widget)
                                    )
                                );
                    });
        }

        template <typename UFunc>
        auto provide(WidgetValueProvider<UFunc>&& provider) &&
        {
            return widgetValueProvider(
                    [mapper=std::move(func), provider=std::move(provider.func)]
                    (auto widget, auto data) mutable
                    {
                        return std::move(provider)(
                                std::move(mapper)(
                                    std::move(widget)
                                    ),
                                std::move(data)
                                );
                    });
        }

        template <typename UFunc, typename... UFuncs>
        auto provide(
                WidgetValueProvider<UFunc>&& provider,
                WidgetValueProvider<UFuncs>&&... providers
                )
        {
            return std::move(provider).provide(std::move(providers)...);
        }

        template <typename... Ts>
        auto provideValues(Ts&&... ts) &&
        {
            return widgetValueProvider(
                    [self=std::move(func),
                    values=btl::cloneOnCopy(std::make_tuple(std::forward<Ts>(ts)...))]
                    (auto widget, auto data) mutable
                    {
                        auto widget2 = std::move(self)(std::move(widget));

                        return std::make_pair(
                                std::move(widget2),
                                btl::cloneOnCopy(std::tuple_cat(
                                    std::move(data),
                                    std::move(*values)
                                    ))
                                );
                    });
        }

    };

    inline auto makeWidgetMap()
    {
        return widgetMap([](auto w) { return w; });
    }
}

