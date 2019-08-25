#pragma once

#include "widget.h"
#include "widgetvaluemapper.h"
#include "widgetvalueconsumer.h"
#include "widgetmap.h"

#include <pmr/new_delete_resource.h>

#include <type_traits>
#include <utility>

namespace reactive
{
    template <typename TFunc>
    struct WidgetValueProvider;

    template <typename TFunc>
    auto widgetValueProvider(TFunc&& func);

    template <typename TFunc>
    struct WidgetMapWrapper;

    namespace detail
    {
        template <typename TFunc>
        auto widgetMap2(TFunc func)
        {
            return WidgetMapWrapper<std::decay_t<TFunc>>{
                std::move(func)
            };
        }
    } // namespace detail

    template <typename TFunc>
    struct WidgetValueProvider
    {
        std::decay_t<TFunc> func;

        using PairType = std::result_of_t<TFunc(Widget, std::tuple<>)>;
        using WidgetType = typename PairType::first_type;
        using DataType = typename PairType::second_type;

        static_assert(IsWidget<WidgetType>::value, "");


        template <typename T, typename U>
        auto operator()(T&& widget, U&& data)
            -> std::decay_t<
            decltype(func(std::forward<T>(widget), std::forward<U>(data)))
            >
        {
            return func(std::forward<T>(widget), std::forward<U>(data));
        }

        template <typename UFunc>
        auto consume(WidgetValueConsumer<UFunc>&& consumer) &&
        {
            return detail::widgetMap2(
                    [self=std::move(func), consumer=std::move(consumer.func)]
                    (auto widget) mutable
                    {
                        auto pair = std::move(self)(
                                std::move(widget),
                                std::tuple<>()
                                );

                        return std::move(consumer)(
                                std::move(pair.first),
                                std::move(pair.second)
                                );
                    });
        }

        template <typename UFunc>
        auto bindWidgetMap(UFunc&& func) &&
        {
            return std::move(*this)
                .consume(widgetValueConsumer(
                [func=std::forward<UFunc>(func)](auto widget, auto data) mutable
                {
                    auto m = std::apply(func, std::move(data));

                    return std::move(m)(std::move(widget));
                }));
        }

        /*
        template <typename UFunc>
        auto operator>>(WidgetValueMapper<UFunc>&& rhs) &&
        {
            return std::move(*this).mapValues(std::move(rhs));
        }
        */

        template <typename UFunc>
        auto provide(WidgetValueProvider<UFunc>&& provider) &&
        {
            return widgetValueProvider(
                    [self=std::move(func), provider=std::move(provider.func)]
                    (auto widget, auto data) mutable
                    {
                        auto pair = std::move(self)(
                                std::move(widget),
                                std::move(data)
                                );

                        return std::move(provider)(
                                std::move(pair.first),
                                std::move(pair.second)
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
                        auto pair = std::move(self)(
                                std::move(widget),
                                std::move(data)
                                );

                        return std::make_pair(
                                std::move(pair.first),
                                std::tuple_cat(
                                    std::move(pair.second),
                                    std::move(*values)
                                    )
                                );
                    });
        }

        template <typename UFunc>
        auto mapValues(WidgetValueMapper<UFunc>&& mapper) &&
        {
            return widgetValueProvider(
                    [self=std::move(func), rhs=std::move(mapper.func)]
                    (auto widget, auto data) mutable
                    {
                        auto pair = std::move(self)(
                                std::move(widget),
                                std::tuple<>()
                                );

                        auto result = std::move(rhs)(
                                std::move(pair.first),
                                std::move(pair.second)
                                );

                        return std::make_pair(
                                std::move(result.first),
                                std::tuple_cat(
                                    std::move(data),
                                    std::move(result.second)
                                    )
                                );

                    });
        }

        template <typename UFunc>
        auto bindValueProvider(UFunc&& f) &&
        {
            return widgetValueProvider(
                [self=std::move(func), f=std::forward<UFunc>(f)]
                (auto widget, auto data) mutable
                {
                    auto pair = std::move(self)(
                            std::move(widget),
                            std::tuple<>()
                            );

                    auto result = std::apply(
                            std::move(f),
                            std::move(pair.second)
                            );

                    return std::move(result)(
                            std::move(pair.first),
                            std::move(data)
                            );
                });
            /*
            return std::move(*this)
                .mapValues(widgetValueMapper(
                    [f=std::forward<UFunc>(f)](auto widget, auto data) mutable
                    {
                        auto m = std::apply(f, std::move(data));
                        return std::move(m)(std::move(widget), std::tuple<>());
                    }));
                    */
        }
    };

    template <typename TFunc>
    auto widgetValueProvider(TFunc&& func)
    {
        /*
        using ReturnType = decltype(
                std::forward<TFunc>(func)(makeWidget(
                        signal::constant(DrawContext(pmr::new_delete_resource())),
                        signal::constant(avg::Vector2f())
                        ), std::tuple<>())
                );

        static_assert(IsWidget<typename ReturnType::first_type>::value,
                "Return type is not a pair<Widget, std::tuple>");
                */

        return WidgetValueProvider<std::decay_t<TFunc>>{
            std::forward<TFunc>(func)
        };
    }

    template <typename TFunc>
    auto widgetValueProvider(WidgetValueProvider<TFunc> func)
    {
        return std::move(func);
    }

    // Takes a function that consumes the data parameters and returns a
    // widget value provdier.
    template <typename TFunc>
    auto bindWidgetValueProvider(TFunc&& func)
    {
        return widgetValueMapper(
                [func=std::forward<TFunc>(func)](auto widget, auto data) mutable
                {
                    auto m = std::apply(func, std::move(data));
                    return std::move(m.func)(std::move(widget), std::tuple<>());
                });
    }
} // namespace reactive

