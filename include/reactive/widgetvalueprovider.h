#pragma once

#include "widgetvalueconsumer.h"
#include "widgetmap.h"

#include <type_traits>
#include <utility>

namespace reactive
{
    template <typename TFunc>
    struct WidgetValueProvider;

    template <typename TFunc>
    auto widgetValueProvider(TFunc func);

    template <typename TFunc>
    struct WidgetMapWrapper;

    template <typename TFunc>
    struct WidgetValueProvider
    {
        std::decay_t<TFunc> func;

        template <typename UFunc>
        auto operator>>(WidgetValueConsumer<UFunc>&& rhs) &&
        {
            static_assert(IsWidget<
                    std::result_of_t<UFunc(
                        std::result_of_t<TFunc(Widget)>
                        )
                    > >::value
                    , "");

            return widgetMap(
                    [self=std::move(func), rhs=std::move(rhs.func)]
                    (auto widget) mutable
                    {
                        return std::move(rhs)(
                                std::move(self)(
                                    std::move(widget)
                                    )
                                );
                    });
        }

        template <typename UFunc>
        auto operator>>(WidgetValueProvider<UFunc>&& rhs) &&
        {
            return widgetValueProvider(
                    [self=std::move(func), rhs=std::move(rhs.func)]
                    (auto widget) mutable
                    {
                        return std::move(rhs)(
                                std::move(self)(
                                    std::move(widget)
                                    )
                                );
                    });
        }
    };

    template <typename T, typename U>
    auto operator|(WidgetMapWrapper<T>&& mapper, WidgetValueProvider<U>&& provider)
    {
        return widgetValueProvider(
                [mapper=std::move(mapper.func), provider=std::move(provider.func)]
                (auto widget) mutable
                {
                    return std::move(provider)(
                            std::move(mapper)(
                                std::move(widget)
                                )
                            );
                });
    }

    template <typename TFunc>
    auto widgetValueProvider(TFunc func)
    {
        return WidgetValueProvider<std::decay_t<TFunc>>{
            std::move(func)
        };
    }
} // namespace reactive

