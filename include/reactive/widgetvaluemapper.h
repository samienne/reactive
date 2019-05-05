#pragma once

#include <utility>

namespace reactive
{
    template <typename TFunc>
    struct WidgetValueMapper
    {
        std::decay_t<TFunc> func;
    };

    template <typename TFunc>
    auto widgetValueMapper(TFunc func)
    {
        return WidgetValueMapper<std::decay_t<TFunc>>{
            std::move(func)
        };
    }

} // namespace reactive

