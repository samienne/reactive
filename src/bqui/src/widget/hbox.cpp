#include "bqui/widget/hbox.h"

#include "constraintbox.h"

namespace bqui::widget
{

AnyWidget hbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverHbox(std::move(widgets));
}

AnyWidget baselineHbox(bq::signal::ArraySignal<AnyWidget> widgets)
{
    return solverBaselineHbox(std::move(widgets));
}

} // namespace bqui::widget
