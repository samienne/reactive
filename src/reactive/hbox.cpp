#include "hbox.h"

#include "box.h"

namespace reactive
{
    WidgetFactory hbox(std::vector<WidgetFactory> widgets)
    {
        return box<Axis::x>(std::move(widgets));
    }
} // namespace reactive

