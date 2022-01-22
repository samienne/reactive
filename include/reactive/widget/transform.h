#pragma once

#include "instancemodifier.h"

#include <reactive/signal/signal.h>

#include <avg/transform.h>

namespace reactive::widget
{
    template <typename T>
    inline auto transform(Signal<T, avg::Transform> t)
    {
        return makeInstanceModifier([](Instance instance, avg::Transform const& t)
                {
                    return std::move(instance).transform(t);
                },
                std::move(t)
                );
    }
} // namespace reactive::widget

