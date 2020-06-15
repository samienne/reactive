#pragma once

#include "theme.h"

#include "reactive/widgetfactory.h"
#include "reactive/reactivevisibility.h"

#include "reactive/signal/signal.h"

#include <avg/font.h>

#include <string>

namespace reactive::widget
{
    REACTIVE_EXPORT WidgetFactory label(AnySharedSignal<std::string> text);

    inline WidgetFactory label(AnySignal<std::string> text)
    {
        return label(signal::share(std::move(text)));
    }
} // reactive::widget

