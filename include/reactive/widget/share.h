#pragma once

#include "reactive/widgetgetters.h"

#include "reactive/signal/share.h"

#include <btl/visibility.h>
#include <btl/typelist.h>

#include <utility>

namespace reactive::widget
{
    namespace detail
    {
        template <typename TWidget>
        BTL_FORCE_INLINE auto doShare(TWidget widget, btl::TypeList<>)
        {
            return std::move(widget);
        }

        template <typename TWidget, typename T, typename... Ts>
        BTL_FORCE_INLINE auto doShare(TWidget widget, btl::TypeList<T, Ts...>)
        {
            auto s = signal::share(get<T>(widget));
            auto newWidget = doShare(
                    std::move(widget),
                    btl::TypeList<Ts...>()
                    );

            return set(std::move(newWidget), std::move(s));
        }
    }

    template <typename... TTags, typename TWidget>
    auto share(TWidget widget)
    {
        return detail::doShare(
                std::move(widget),
                btl::typelist::unique(btl::TypeList<TTags...>())
                );
    }
} // namespace reactive::widget

