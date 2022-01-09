#pragma once

#include "widgetmodifier.h"

#include <reactive/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    template <typename T>
    inline auto transform(Signal<T, avg::Transform> t)
    {
        return makeWidgetModifier([](Widget widget, avg::Transform const& t)
                {
                    return std::move(widget).transform(t);
                },
                std::move(t)
                );
    }
} // namespace reactive::widget

