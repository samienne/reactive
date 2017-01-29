#pragma once

#include "theme.h"

#include "reactive/signal.h"
#include "reactive/widgetfactory.h"

#include <avg/font.h>

#include <string>

namespace reactive
{
    namespace widget
    {
        WidgetFactory label(Signal<std::string> text);
    }
}

