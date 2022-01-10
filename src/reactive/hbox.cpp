#include "hbox.h"

#include "dynamicbox.h"
#include "box.h"

namespace reactive
{

widget::Builder hbox(std::vector<widget::Builder> widgets)
{
    return box<Axis::x>(std::move(widgets));
}

widget::Builder hbox(AnySignal<std::vector<widget::WidgetObject>> widgets)
{
    return dynamicBox<Axis::x>(std::move(widgets));
}

} // namespace reactive

