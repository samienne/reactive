#pragma once

#include "theme.h"
#include "builder.h"

#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

#include <avg/font.h>

#include <string>

namespace reactive::widget
{
    REACTIVE_EXPORT AnyBuilder label(AnySharedSignal<std::string> text);
    REACTIVE_EXPORT AnyBuilder label(std::string const& text);

    inline AnyBuilder label(AnySignal<std::string> text)
    {
        return label(signal::share(std::move(text)));
    }
} // reactive::widget

