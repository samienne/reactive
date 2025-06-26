#include "bqui/widget/hbox.h"

#include "bqui/widget/box.h"

#include "bqui/dynamicbox.h"

namespace bqui::widget
{

AnyWidget hbox(std::vector<AnyWidget> widgets)
{
    return box<Axis::x>(std::move(widgets));
}

AnyWidget hbox(
        bq::signal::AnySignal<std::vector<std::pair<size_t, AnyWidget>>> widgets)
{
    return dynamicBox<Axis::x>(std::move(widgets));
}

} // namespace reactive

