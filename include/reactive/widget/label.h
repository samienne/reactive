#pragma once

#include "theme.h"

#include "reactive/signal.h"
#include "reactive/widgetfactory.h"

#include <avg/font.h>

#include <string>

namespace reactive::widget
{
    WidgetFactory label(signal2::SharedSignal<std::string> text);

    inline WidgetFactory label(signal2::Signal<std::string> text)
    {
        return label(signal2::share2(std::move(text)));
    }
} // reactive::widget

