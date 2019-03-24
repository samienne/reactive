#include "adder.h"

#include <reactive/widget/label.h>

#include <reactive/vbox.h>

#include <reactive/signal/databind.h>
#include <reactive/signal/constant.h>

#include <string>

using namespace reactive;

reactive::WidgetFactory adder()
{
    Collection<std::string> items;

    items.pushBack("test 1");
    items.pushBack("test 2");
    items.pushBack("test 3");

    auto widgets = signal::dataBind<std::string>(
            items,
            [](Signal<std::string> value, size_t) -> WidgetFactory
            {
                return reactive::widget::label(std::move(value));
            });

    return vbox(std::move(widgets));

    //return reactive::widget::label(signal::constant<std::string>("Adder"));
}

