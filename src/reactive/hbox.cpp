#include "hbox.h"

#include "dynamicbox.h"
#include "box.h"

namespace reactive
{

widget::AnyWidget hbox(std::vector<widget::AnyWidget> widgets)
{
    return box<Axis::x>(std::move(widgets));
}

widget::AnyWidget hbox(
        signal::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>> widgets)
{
    return dynamicBox<Axis::x>(std::move(widgets));
}

} // namespace reactive

