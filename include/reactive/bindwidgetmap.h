#pragma once

#include "widgetmap.h"

namespace reactive
{
    namespace detail
    {
        template <typename TFunc, typename TWidget, typename TTuple, size_t... S>
        auto applyTupleMap(TFunc&& func, TWidget&& widget,
                TTuple&& tuple, std::index_sequence<S...>)
        {
            return std::invoke(
                    std::forward<TFunc>(func),
                    std::forward<TWidget>(widget),
                    std::get<S>(std::forward<TTuple>(tuple))...
                    );
        }
    }

    template <typename... TTags, typename TFunc, typename =
        std::enable_if_t<
            IsWidgetMap<std::result_of_t<
                TFunc(SharedSignal<typename TTags::Type>...)
            >>::value
        >>
    auto bindWidgetMap(TFunc&& widgetMap)
    {
        using Tags = btl::UniqueType<
            btl::MapListType<
                DependentType,
                btl::TypeList<TTags...>
                >
            >;

        return mapWidget([widgetMap=std::forward<TFunc>(widgetMap)]
            (auto widget)
            {
                auto widget2 = detail::doShare(std::move(widget), Tags());

                auto wm = widgetMap(signal::share(get<TTags>(widget2)...));
                return std::move(widget2) | std::move(wm);
            });
    }
} // namespace reactive

