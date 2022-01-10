#include "vbox.h"

#include "box.h"
#include "dynamicbox.h"

#include "widget/addwidgets.h"

#include "signal/map.h"
#include "signal/combine.h"
#include "signal/join.h"

namespace reactive
{

widget::Builder vbox(std::vector<widget::Builder> widgets)
{
    return box<Axis::y>(std::move(widgets));
}


widget::Builder vbox(AnySignal<std::vector<widget::WidgetObject>> widgets)
{
    return dynamicBox<Axis::y>(std::move(widgets));
}

} // namespace reactive

