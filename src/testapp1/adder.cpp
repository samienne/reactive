#include "adder.h"

#include <reactive/widget/label.h>

#include <reactive/signal/constant.h>

#include <string>

using namespace reactive;

reactive::WidgetFactory adder()
{
    return reactive::widget::label(signal::constant<std::string>("Adder"));
}

