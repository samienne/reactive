#pragma once

#include "theme.h"

#include "reactive/signal.h"
#include "reactive/widgetfactory.h"

#include <avg/font.h>

#include <string>

namespace reactive::widget
{
    WidgetFactory label(SharedSignal<std::string> text);

    inline WidgetFactory label(Signal<std::string> text)
    {
        return label(signal::share(std::move(text)));
    }
} // reactive::widget

