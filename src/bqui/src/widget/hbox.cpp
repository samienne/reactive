#include "bqui/widget/hbox.h"

#include "bqui/widget/box.h"

namespace bqui::widget
{

AnyWidget hbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return box<Axis::x>(std::move(widgets));
}

} // namespace bqui::widget

