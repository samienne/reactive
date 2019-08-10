#pragma once

#include "widgetvaluemapper.h"
#include "widgetvalueconsumer.h"
#include "widgetmap.h"

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

    template <typename TFunc>
    struct WidgetValueProvider
    {
        std::decay_t<TFunc> func;

        template <typename T, typename U>
        auto operator()(T&& widget, U&& data)
            -> std::decay_t<
            decltype(func(std::forward<T>(widget), std::forward<U>(data)))
            >
        {
            return func(std::forward<T>(widget), std::forward<U>(data));
        }

        template <typename UFunc>
        auto operator>>(WidgetValueConsumer<UFunc>&& rhs) &&
        {
            using PairType = std::result_of_t<TFunc(Widget, std::tuple<>)>;
            using WidgetType = typename PairType::first_type;
            //using DataType = typename PairType::second_type;

            static_assert(IsWidget<WidgetType>::value, "");

            return widgetMap(
                    [self=std::move(func), rhs=std::move(rhs.func)]
                    (auto widget) mutable
                    {
                        auto pair = std::move(self)(
                                std::move(widget),
                                std::tuple<>()
                                );

                        return std::move(rhs)(
                                std::move(pair.first),
                                std::move(pair.second)
                                );
                    });
        }

        template <typename UFunc>
        auto operator>>(WidgetValueMapper<UFunc>&& rhs) &&
        {
            using PairType = std::result_of_t<TFunc(Widget, std::tuple<>)>;
            using WidgetType = typename PairType::first_type;
            //using DataType = typename PairType::second_type;

            static_assert(IsWidget<WidgetType>::value, "");

            return widgetValueProvider(
                    [self=std::move(func), rhs=std::move(rhs.func)]
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
        auto operator>>(WidgetValueProvider<UFunc>&& rhs) &&
        {
            return widgetValueProvider(
                    [self=std::move(func), rhs=std::move(rhs.func)]
                    (auto widget, auto data) mutable
                    {
                        auto pair = std::move(self)(
                                std::move(widget),
                                std::move(data)
                                );

                        return std::move(rhs)(
                                std::move(pair.first),
                                std::move(pair.second)
                                );
                    });
        }
    };

    template <typename T, typename U>
    auto operator|(WidgetMapWrapper<T>&& mapper, WidgetValueProvider<U>&& provider)
    {
        return widgetValueProvider(
                [mapper=std::move(mapper.func), provider=std::move(provider.func)]
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

