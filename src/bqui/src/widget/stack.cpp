#include "bqui/widget/stack.h"

#include "constraintbox.h"

namespace bqui::widget
{
    AnyWidget stack(std::vector<AnyWidget> widgets)
    {
        return solverStack(std::move(widgets));
    }
}
