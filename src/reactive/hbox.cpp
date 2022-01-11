#include "hbox.h"

#include "dynamicbox.h"
#include "box.h"

namespace reactive
{

widget::AnyBuilder hbox(std::vector<widget::AnyBuilder> widgets)
{
    return box<Axis::x>(std::move(widgets));
}

widget::AnyBuilder hbox(AnySignal<std::vector<widget::WidgetObject>> widgets)
{
    return dynamicBox<Axis::x>(std::move(widgets));
}

} // namespace reactive

