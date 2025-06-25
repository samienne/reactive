#include "bqui/widget/vbox.h"

#include "bqui/widget/box.h"

#include "bqui/dynamicbox.h"

#include <bq/signal/signal.h>

namespace bqui::widget
{

AnyWidget vbox(std::vector<AnyWidget> widgets)
{
    return box<Axis::y>(std::move(widgets));
}

AnyWidget vbox(bq::signal::AnySignal<std::vector<std::pair<size_t,
        AnyWidget>>> widgets)
{
    return dynamicBox<Axis::y>(std::move(widgets));
}

}

