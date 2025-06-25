#pragma once

#include "widgetmodifier.h"

#include <bq/signal/signal.h>

#include <avg/transform.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyInstanceModifier transformBuilder(
            bq::signal::AnySignal<avg::Transform> t);
    BQUI_EXPORT AnyWidgetModifier transform(
            bq::signal::AnySignal<avg::Transform> t);
}

