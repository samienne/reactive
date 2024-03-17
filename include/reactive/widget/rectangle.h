#pragma once

#include "reactive/widget/widget.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

#include <optional>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget rectangle();
    REACTIVE_EXPORT AnyWidget rectangle(
            AnySignal<float> cornerRadius,
            std::optional<AnySignal<avg::Brush>> brush = std::nullopt,
            std::optional<AnySignal<avg::Pen>> pen = std::nullopt
            );
} // namespace reactive::widget

