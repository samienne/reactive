#include "bqui/widget/vbox.h"

#include "constraintbox.h"

namespace bqui::widget
{

AnyWidget vbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverVbox(std::move(widgets));
}

} // namespace bqui::widget

