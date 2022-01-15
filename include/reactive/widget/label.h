#pragma once

#include "widget.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

#include <string>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyWidget label(AnySharedSignal<std::string> text);
    REACTIVE_EXPORT AnyWidget label(std::string const& text);

    inline AnyWidget label(AnySignal<std::string> text)
    {
        return label(signal::share(std::move(text)));
    }
} // reactive::widget

