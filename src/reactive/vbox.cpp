#include "vbox.h"

#include "box.h"
#include "dynamicbox.h"

#include "widget/addwidgets.h"

#include "signal/map.h"
#include "signal/combine.h"
#include "signal/join.h"

namespace reactive
{

WidgetFactory vbox(std::vector<WidgetFactory> widgets)
{
    return box<Axis::y>(std::move(widgets));
}


WidgetFactory vbox(Signal<std::vector<widget::WidgetObject>> widgets)
{
    return dynamicBox<Axis::y>(std::move(widgets));
}

} // namespace reactive

