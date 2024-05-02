#include "vbox.h"

#include "box.h"
#include "dynamicbox.h"

#include "signal2/signal.h"

namespace reactive
{

widget::AnyWidget vbox(std::vector<widget::AnyWidget> widgets)
{
    return box<Axis::y>(std::move(widgets));
}

widget::AnyWidget vbox(signal2::AnySignal<std::vector<std::pair<size_t,
        widget::AnyWidget>>> widgets)
{
    return dynamicBox<Axis::y>(std::move(widgets));
}

} // namespace reactive

