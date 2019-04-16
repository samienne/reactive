#include "hbox.h"

#include "dynamicbox.h"
#include "box.h"

namespace reactive
{

WidgetFactory hbox(std::vector<WidgetFactory> widgets)
{
    return box<Axis::x>(std::move(widgets));
}

WidgetFactory hbox(Signal<std::vector<widget::WidgetObject>> widgets)
{
    return dynamicBox<Axis::x>(std::move(widgets));
}

} // namespace reactive

