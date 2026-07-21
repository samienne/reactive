#include "bqui/widget/vbox.h"

#include "bqui/widget/box.h"

namespace bqui::widget
{

AnyWidget vbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return box<Axis::y>(std::move(widgets));
}

} // namespace bqui::widget

