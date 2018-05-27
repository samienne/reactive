#pragma once

#include "theme.h"

#include "reactive/signal.h"
#include "reactive/widgetfactory.h"

#include <avg/font.h>

#include <btl/visibility.h>

#include <string>

namespace reactive::widget
{
    BTL_VISIBLE WidgetFactory label(SharedSignal<std::string> text);

    BTL_VISIBLE inline WidgetFactory label(Signal<std::string> text)
    {
        return label(signal::share(std::move(text)));
    }
} // reactive::widget

