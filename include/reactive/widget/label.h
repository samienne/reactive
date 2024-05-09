#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

#include <string>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget label(signal::AnySignal<std::string> text);
    REACTIVE_EXPORT AnyWidget label(std::string const& text);
} // reactive::widget

