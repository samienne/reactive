#include "vbox.h"

#include "box.h"

namespace reactive
{

WidgetFactory vbox(std::vector<WidgetFactory> widgets)
{
    return box<Axis::y>(std::move(widgets));
}

} // namespace reactive

