#include "vbox.h"

#include "box.h"
#include "dynamicbox.h"

#include "widget/addwidgets.h"

#include "signal/map.h"
#include "signal/combine.h"
#include "signal/join.h"

namespace reactive
{

widget::AnyBuilder vbox(std::vector<widget::AnyBuilder> widgets)
{
    return box<Axis::y>(std::move(widgets));
}


widget::AnyBuilder vbox(AnySignal<std::vector<widget::WidgetObject>> widgets)
{
    return dynamicBox<Axis::y>(std::move(widgets));
}

} // namespace reactive

