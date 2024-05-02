#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal2/signal.h"

#include <string>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget label(signal2::AnySignal<std::string> text);
    REACTIVE_EXPORT AnyWidget label(std::string const& text);
} // reactive::widget

