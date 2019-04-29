#pragma once

#include <utility>

namespace reactive
{
    template <typename TFunc>
    struct WidgetValueConsumer
    {
        std::decay_t<TFunc> func;
    };

    template <typename TFunc>
    auto widgetValueConsumer(TFunc func)
    {
        return WidgetValueConsumer<std::decay_t<TFunc>>{
            std::move(func)
        };
    }

} // namespace reactive

