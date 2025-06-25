#pragma once

#include "widgetmodifier.h"

#include <bq/stream/stream.h>

namespace bqui::modifier
{
    BQUI_EXPORT AnyWidgetModifier focusOn(bq::stream::Stream<bool> stream);
}

